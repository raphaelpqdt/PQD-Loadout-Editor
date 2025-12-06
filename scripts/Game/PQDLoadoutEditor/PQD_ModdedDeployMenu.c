// PQD Loadout Editor - Modded Deploy Menu Components
// Author: PQD Team
// Version: 1.0.0
// Description: Extends the deploy menu to properly handle PQD custom loadouts
// Note: PQD loadouts work via modded SCR_PlayerArsenalLoadout with m_sPQDSlotId

//------------------------------------------------------------------------------------------------
//! Modded LoadoutRequestUIComponent to fix NULL pointer issues with preview widget
modded class SCR_LoadoutRequestUIComponent : ScriptedWidgetComponent
{
	//------------------------------------------------------------------------------------------------
	//! Override: Set loadout preview with NULL protection
	override protected void SetLoadoutPreview(SCR_BasePlayerLoadout loadout)
	{
		// Critical NULL checks before calling base
		if (!loadout)
			return;
		
		// Check if preview component exists
		if (!m_PreviewComp)
			return;
		
		// Check if loadout preview widget exists - this is the main cause of NULL errors
		if (!m_wLoadoutPreview)
		{
			Print("[PQD] SetLoadoutPreview: m_wLoadoutPreview is null, skipping preview", LogLevel.DEBUG);
			return;
		}
		
		// Now safe to call base implementation
		super.SetLoadoutPreview(loadout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override: Refresh loadout preview with NULL protection
	override void RefreshLoadoutPreview()
	{
		// Check if player loadout component exists
		if (!m_PlyLoadoutComp)
			return;
		
		SCR_BasePlayerLoadout loadout = m_PlyLoadoutComp.GetLoadout();
		if (!loadout)
			return;
		
		// Check if preview widget exists
		if (!m_wLoadoutPreview)
		{
			Print("[PQD] RefreshLoadoutPreview: m_wLoadoutPreview is null, skipping", LogLevel.DEBUG);
			
			// Still update other UI elements
			if (m_wLoadoutName)
				m_wLoadoutName.SetText(loadout.GetLoadoutName());
			
			if (m_wExpandButtonName)
				m_wExpandButtonName.SetText(loadout.GetLoadoutName());
			
			if (m_LoadoutSelector)
				m_LoadoutSelector.SetSelected(loadout);
			
			return;
		}
		
		// Safe to call base
		super.RefreshLoadoutPreview();
	}
}

//------------------------------------------------------------------------------------------------
//! Modded LoadoutGallery to support PQD loadout selection
modded class SCR_LoadoutGallery : SCR_GalleryComponent
{
	//------------------------------------------------------------------------------------------------
	//! Override: Add item to gallery - adds special handling for PQD loadouts
	override int AddItem(SCR_BasePlayerLoadout loadout, bool enabled = true)
	{
		// Check if this is a PQD loadout and log it
		SCR_PlayerArsenalLoadout arsenalLoadout = SCR_PlayerArsenalLoadout.Cast(loadout);
		if (arsenalLoadout && arsenalLoadout.IsPQDLoadoutSlot())
		{
			Print(string.Format("[PQD] LoadoutGallery: Adding PQD loadout slot %1", arsenalLoadout.GetPQDSlotId()), LogLevel.DEBUG);
		}
		
		// Call the original implementation
		return super.AddItem(loadout, enabled);
	}
}

//------------------------------------------------------------------------------------------------
//! Modded PlayerLoadoutComponent to handle PQD loadout assignment
modded class SCR_PlayerLoadoutComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! Override: Request loadout to support PQD custom loadouts
	override bool RequestLoadout(SCR_BasePlayerLoadout loadout)
	{
		// Check if this is a PQD custom loadout
		SCR_PlayerArsenalLoadout arsenalLoadout = SCR_PlayerArsenalLoadout.Cast(loadout);
		if (arsenalLoadout && arsenalLoadout.IsPQDLoadoutSlot())
		{
			string slotId = arsenalLoadout.GetPQDSlotId();
			string factionKey = arsenalLoadout.GetFactionKey();
			
			Print(string.Format("[PQD] RequestLoadout: PQD loadout slot %1, faction %2 requested", slotId, factionKey), LogLevel.NORMAL);
			
			// Send PQD selection to server via RPC
			// The server needs to know this is a PQD selection BEFORE spawning
			Rpc(Rpc_NotifyPQDSelection_S, slotId, factionKey);
		}
		
		return super.RequestLoadout(loadout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC: Notify server about PQD loadout selection
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void Rpc_NotifyPQDSelection_S(string slotId, string factionKey)
	{
		PlayerController pc = GetPlayerController();
		if (!pc)
		{
			Print("[PQD] Rpc_NotifyPQDSelection_S: PlayerController not found", LogLevel.ERROR);
			return;
		}
		
		int playerId = pc.GetPlayerId();
		Print(string.Format("[PQD] Rpc_NotifyPQDSelection_S: Player %1 selected PQD slot %2, faction %3", 
			playerId, slotId, factionKey), LogLevel.NORMAL);
		
		// Store this selection on the server
		PQD_ServerLoadoutSelection.SetPlayerSelection(playerId, slotId, factionKey);
		
		// Pre-load and cache the loadout data NOW to avoid race conditions with spawn
		// This ensures data is ready when OnPlayerSpawned is called
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			PQD_LoadoutStorageComponent storageComp = PQD_LoadoutStorageComponent.Cast(gameMode.FindComponent(PQD_LoadoutStorageComponent));
			if (storageComp)
			{
				int slotIndex = PQD_GetSlotIndex(slotId);
				if (slotIndex >= 0)
				{
					string prefab, loadoutData, requiredRank;
					float cost;
					
					if (storageComp.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank))
					{
						// Add to pending loadout manager to guarantee it's available when spawn happens
						PQD_PendingLoadoutManager.AddPendingLoadout(playerId, slotId, loadoutData, factionKey, prefab);
						Print(string.Format("[PQD] Pre-cached loadout data for player %1, slot %2 (data size: %3 bytes)", 
							playerId, slotId, loadoutData.Length()), LogLevel.NORMAL);
					}
					else
					{
						Print(string.Format("[PQD] Warning: Could not pre-cache loadout data for player %1, slot %2 - will try on spawn", 
							playerId, slotId), LogLevel.WARNING);
					}
				}
				else
				{
					Print(string.Format("[PQD] Warning: Invalid slot ID '%1'", slotId), LogLevel.WARNING);
				}
			}
			else
			{
				Print("[PQD] Warning: PQD_LoadoutStorageComponent not found on GameMode", LogLevel.WARNING);
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Modded LoadoutPreviewComponent to properly preview PQD loadouts with saved equipment
modded class SCR_LoadoutPreviewComponent : ScriptedWidgetComponent
{
	//------------------------------------------------------------------------------------------------
	//! Override: Set preview for loadout - handles PQD loadout preview with actual saved items
	override IEntity SetPreviewedLoadout(notnull SCR_BasePlayerLoadout loadout, PreviewRenderAttributes attributes = null)
	{
		// Check if this is a PQD loadout
		SCR_PlayerArsenalLoadout arsenalLoadout = SCR_PlayerArsenalLoadout.Cast(loadout);
		if (!arsenalLoadout || !arsenalLoadout.IsPQDLoadoutSlot())
			return super.SetPreviewedLoadout(loadout, attributes);
		
		Print(string.Format("[PQD] Preview PQD loadout slot %1", arsenalLoadout.GetPQDSlotId()), LogLevel.DEBUG);
		
		// Make sure we can reload
		if (!m_bReloadLoadout)
			return null;
		
		// Check if preview widget is valid - CRITICAL null check
		if (!m_wPreview)
		{
			Print("[PQD] Preview: m_wPreview is null, cannot show preview", LogLevel.WARNING);
			return null;
		}
		
		// Get the preview manager
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[PQD] Preview: World is null", LogLevel.WARNING);
			return null;
		}
		m_PreviewManager = world.GetItemPreviewManager();
		
		if (!m_PreviewManager)
		{
			Resource res = Resource.Load(m_sPreviewManager);
			if (res.IsValid())
				GetGame().SpawnEntityPrefabLocal(res, world);
			
			m_PreviewManager = world.GetItemPreviewManager();
			if (!m_PreviewManager)
				return null;
		}
		
		// Get the base entity for preview
		ResourceName resName = loadout.GetLoadoutResource();
		if (resName.IsEmpty())
		{
			Print("[PQD] Preview: Loadout resource is empty", LogLevel.WARNING);
			return null;
		}
		
		IEntity previewedEntity = m_PreviewManager.ResolvePreviewEntityForPrefab(resName);
		if (!previewedEntity)
		{
			Print(string.Format("[PQD] Preview: Failed to resolve entity for %1", resName), LogLevel.WARNING);
			return null;
		}
		
		// Get gamemode for fallback storage access
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			if (m_wPreview)
				m_PreviewManager.SetPreviewItemFromPrefab(m_wPreview, resName, attributes);
			return previewedEntity;
		}
		
		// Get the local player's info
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
		{
			if (m_wPreview)
				m_PreviewManager.SetPreviewItemFromPrefab(m_wPreview, resName, attributes);
			return previewedEntity;
		}
		
		int playerId = pc.GetPlayerId();
		string factionKey = arsenalLoadout.GetFactionKey();
		
		// Get slot index from slot ID
		string slotId = arsenalLoadout.GetPQDSlotId();
		int slotIndex = -1;
		if (slotId.Length() > 4)
			slotIndex = slotId.Substring(4, slotId.Length() - 4).ToInt() - 1;
		
		if (slotIndex < 0)
		{
			if (m_wPreview)
				m_PreviewManager.SetPreviewItemFromPrefab(m_wPreview, resName, attributes);
			return previewedEntity;
		}
		
		// Get the loadout data - use client cache first (for dedicated servers)
		string prefab, loadoutData, requiredRank;
		float cost;
		
		// Try to get from client cache first (populated by server RPC)
		bool hasData = PQD_ClientLoadoutCache.GetLoadoutData(factionKey, slotIndex, prefab, loadoutData, cost, requiredRank);
		
		// If not in cache, try storage component (works for local/listen server)
		if (!hasData)
		{
			PQD_LoadoutStorageComponent storageComp = PQD_LoadoutStorageComponent.Cast(gameMode.FindComponent(PQD_LoadoutStorageComponent));
			if (storageComp)
			{
				hasData = storageComp.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank);
			}
		}
		
		if (!hasData)
		{
			Print(string.Format("[PQD] Preview: No loadout data for faction %1 slot %2", factionKey, slotIndex), LogLevel.DEBUG);
			if (m_wPreview)
				m_PreviewManager.SetPreviewItemFromPrefab(m_wPreview, resName, attributes);
			return previewedEntity;
		}
		
		Print(string.Format("[PQD] Preview: Using loadout data for %1 slot %2 (size: %3)", factionKey, slotIndex, loadoutData.Length()), LogLevel.DEBUG);
		
		// Convert our JSON data to SCR_PlayerLoadoutData
		SCR_PlayerLoadoutData playerLoadoutData = PQD_ConvertJsonToLoadoutData(loadoutData);
		if (!playerLoadoutData || (playerLoadoutData.Clothings.IsEmpty() && playerLoadoutData.Weapons.IsEmpty()))
		{
			Print("[PQD] Preview: No clothing/weapon data found", LogLevel.DEBUG);
			if (m_wPreview)
				m_PreviewManager.SetPreviewItemFromPrefab(m_wPreview, resName, attributes);
			return previewedEntity;
		}
		
		Print(string.Format("[PQD] Preview: Applying %1 clothes, %2 weapons", 
			playerLoadoutData.Clothings.Count(), playerLoadoutData.Weapons.Count()), LogLevel.NORMAL);
		
		// Delete all children (like WCS does)
		DeleteChildrens(previewedEntity, false);
		
		// Apply clothing - try EquipedLoadoutStorageComponent first (used in preview)
		EquipedLoadoutStorageComponent loadoutStorage = EquipedLoadoutStorageComponent.Cast(previewedEntity.FindComponent(EquipedLoadoutStorageComponent));
		if (loadoutStorage)
		{
			Print(string.Format("[PQD] EquipedLoadoutStorageComponent found with %1 slots", loadoutStorage.GetSlotsCount()), LogLevel.NORMAL);
			
			for (int i = 0; i < playerLoadoutData.Clothings.Count(); i++)
			{
				SCR_ClothingLoadoutData clothingData = playerLoadoutData.Clothings[i];
				InventoryStorageSlot slot = loadoutStorage.GetSlot(clothingData.SlotIdx);
				if (!slot)
				{
					Print(string.Format("[PQD] No slot at index %1", clothingData.SlotIdx), LogLevel.WARNING);
					continue;
				}
				
				Resource resource = Resource.Load(clothingData.ClothingPrefab);
				if (!resource || !resource.IsValid())
				{
					Print(string.Format("[PQD] Failed to load resource: %1", clothingData.ClothingPrefab), LogLevel.WARNING);
					continue;
				}
				
				IEntity cloth = GetGame().SpawnEntityPrefabLocal(resource, previewedEntity.GetWorld());
				if (!cloth)
				{
					Print(string.Format("[PQD] Failed to spawn cloth: %1", clothingData.ClothingPrefab), LogLevel.WARNING);
					continue;
				}
				
				slot.AttachEntity(cloth);
				Print(string.Format("[PQD] Attached cloth at slot %1: %2", clothingData.SlotIdx, clothingData.ClothingPrefab), LogLevel.NORMAL);
			}
		}
		else
		{
			Print("[PQD] EquipedLoadoutStorageComponent NOT found on preview entity!", LogLevel.WARNING);
		}
		
		// Apply weapons
		EquipedWeaponStorageComponent weaponStorage = EquipedWeaponStorageComponent.Cast(previewedEntity.FindComponent(EquipedWeaponStorageComponent));
		if (weaponStorage)
		{
			Print(string.Format("[PQD] EquipedWeaponStorageComponent found with %1 slots", weaponStorage.GetSlotsCount()), LogLevel.NORMAL);
			
			for (int i = 0; i < playerLoadoutData.Weapons.Count(); i++)
			{
				SCR_WeaponLoadoutData weaponData = playerLoadoutData.Weapons[i];
				InventoryStorageSlot slot = weaponStorage.GetSlot(weaponData.SlotIdx);
				if (!slot)
				{
					Print(string.Format("[PQD] No weapon slot at index %1", weaponData.SlotIdx), LogLevel.WARNING);
					continue;
				}
				
				Resource resource = Resource.Load(weaponData.WeaponPrefab);
				if (!resource || !resource.IsValid())
				{
					Print(string.Format("[PQD] Failed to load weapon resource: %1", weaponData.WeaponPrefab), LogLevel.WARNING);
					continue;
				}
				
				IEntity weapon = GetGame().SpawnEntityPrefabLocal(resource, previewedEntity.GetWorld());
				if (!weapon)
				{
					Print(string.Format("[PQD] Failed to spawn weapon: %1", weaponData.WeaponPrefab), LogLevel.WARNING);
					continue;
				}
				
				slot.AttachEntity(weapon);
				Print(string.Format("[PQD] Attached weapon at slot %1: %2", weaponData.SlotIdx, weaponData.WeaponPrefab), LogLevel.NORMAL);
			}
		}
		else
		{
			Print("[PQD] EquipedWeaponStorageComponent NOT found on preview entity!", LogLevel.WARNING);
		}
		
		// Select default weapon for proper pose
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(previewedEntity.FindComponent(BaseWeaponManagerComponent));
		if (weaponManager)
		{
			int weaponDefaultIndex = weaponManager.GetDefaultWeaponIndex();
			if (weaponDefaultIndex > -1)
			{
				array<WeaponSlotComponent> outSlots = {};
				weaponManager.GetWeaponsSlots(outSlots);
				foreach (WeaponSlotComponent weaponSlot : outSlots)
				{
					if (weaponSlot.GetWeaponSlotIndex() == weaponDefaultIndex)
					{
						weaponManager.SelectWeapon(weaponSlot);
						break;
					}
				}
			}
		}
		
		// Set the preview
		if (m_wPreview)
			m_PreviewManager.SetPreviewItem(m_wPreview, previewedEntity, attributes, true);
		
		return previewedEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Convert JSON loadout data to SCR_PlayerLoadoutData structure
	protected SCR_PlayerLoadoutData PQD_ConvertJsonToLoadoutData(string loadoutData)
	{
		if (loadoutData.IsEmpty())
			return null;
		
		SCR_PlayerLoadoutData result = new SCR_PlayerLoadoutData();
		
		// Parse the JSON
		SCR_JsonLoadContext context = new SCR_JsonLoadContext();
		if (!context.ImportFromString(loadoutData))
		{
			Print("[PQD] ConvertJsonToLoadoutData: Failed to parse JSON", LogLevel.WARNING);
			return null;
		}
		
		// Start reading the arsenalLoadout object
		if (!context.StartObject(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_KEY))
		{
			Print("[PQD] ConvertJsonToLoadoutData: No arsenalLoadout object", LogLevel.WARNING);
			return null;
		}
		
		// Read storages array
		int storageCount;
		if (!context.StartArray("storages", storageCount))
		{
			context.EndObject();
			return result;
		}
		
		Print(string.Format("[PQD] ConvertJsonToLoadoutData: Found %1 storages", storageCount), LogLevel.DEBUG);
		
		// Process each storage
		for (int nStorage = 0; nStorage < storageCount; nStorage++)
		{
			if (!context.StartObject())
				continue;
			
			// Read storage ID to determine type
			string storageId;
			if (!context.ReadValue("id", storageId))
			{
				context.EndObject();
				continue;
			}
			
			Print(string.Format("[PQD] Processing storage: %1", storageId), LogLevel.DEBUG);
			
			// Determine storage type based on ID
			// Weapons are in EquipedWeaponStorageComponent
			// Clothes are in SCR_CharacterInventoryStorageComponent (slots 0-5: head, jacket, vest, pants, boots, backpack)
			bool isWeaponStorage = storageId.Contains("EquipedWeaponStorageComponent");
			bool isClothingStorage = storageId.Contains("SCR_CharacterInventoryStorageComponent");
			
			// Read slots
			int slotCount;
			if (!context.StartMap("slots", slotCount))
			{
				context.EndObject();
				continue;
			}
			
			Print(string.Format("[PQD] Storage has %1 slots", slotCount), LogLevel.DEBUG);
			
			// Process each slot
			for (int i = 0; i < slotCount; i++)
			{
				string slotIdxStr;
				if (!context.ReadMapKey(i, slotIdxStr))
					continue;
				
				int slotIdx = slotIdxStr.ToInt(-1);
				if (slotIdx == -1)
					continue;
				
				if (!context.StartObject(slotIdxStr))
					continue;
				
				// Read the prefab - stored as "prefab" key
				ResourceName itemPrefab;
				if (context.ReadValue("prefab", itemPrefab) && !itemPrefab.IsEmpty())
				{
					Print(string.Format("[PQD] Found item at slot %1: %2", slotIdx, itemPrefab), LogLevel.DEBUG);
					
					if (isWeaponStorage)
					{
						SCR_WeaponLoadoutData weaponData = new SCR_WeaponLoadoutData();
						weaponData.SlotIdx = slotIdx;
						weaponData.WeaponPrefab = itemPrefab;
						weaponData.Active = (slotIdx == 0);
						result.Weapons.Insert(weaponData);
					}
					else if (isClothingStorage)
					{
						SCR_ClothingLoadoutData clothingData = new SCR_ClothingLoadoutData();
						clothingData.SlotIdx = slotIdx;
						clothingData.ClothingPrefab = itemPrefab;
						result.Clothings.Insert(clothingData);
					}
				}
				
				context.EndObject();
			}
			
			context.EndMap();
			context.EndObject();
		}
		
		context.EndArray();
		context.EndObject();
		
		Print(string.Format("[PQD] ConvertJsonToLoadoutData: Found %1 clothes, %2 weapons", 
			result.Clothings.Count(), result.Weapons.Count()), LogLevel.DEBUG);
		
		return result;
	}
}
