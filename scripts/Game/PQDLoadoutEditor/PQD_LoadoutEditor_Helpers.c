// PQD Loadout Editor - Helper Functions
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
//! Global function to convert slot ID string to index (Slot1 -> 0, Slot2 -> 1, etc.)
//! This is a global function so it can be called from any class
int PQD_GetSlotIndex(string slotId)
{
	if (slotId == "Slot1") return 0;
	if (slotId == "Slot2") return 1;
	if (slotId == "Slot3") return 2;
	if (slotId == "Slot4") return 3;
	if (slotId == "Slot5") return 4;
	return -1;
}

//------------------------------------------------------------------------------------------------
sealed class PQD_Helpers
{
	static ItemPreviewManagerEntity m_previewManager;
	static ResourceName m_sDialogPresets = "{2FA28C928044D25A}Configs/PQDLoadoutEditor/PQD_Dialogs.conf";
	static ref SCR_ConfigurableDialogUi m_dialog;
	static ref map<ResourceName, ref PQD_ArsenalItemDetails> m_ArsenalItemDetails = new map<ResourceName, ref PQD_ArsenalItemDetails>;
	
	//------------------------------------------------------------------------------------------------
	// Get RplId from entity
	static RplId GetEntityRplId(IEntity ent)
	{
		if (!ent)
			return RplId.Invalid();
		
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();
		
		return rpl.Id();
	}
	
	//------------------------------------------------------------------------------------------------
	// Get entity from RplId
	static IEntity GetEntityFromRplId(RplId rplId)
	{
		if (!rplId.IsValid())
			return null;
		
		RplComponent item = RplComponent.Cast(Replication.FindItem(rplId));
		if (!item)
			return null;
		
		return item.GetEntity();
	}
	
	//------------------------------------------------------------------------------------------------
	// Check if local player is admin
	static bool IsLocalPlayerAdmin()
	{
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		return SCR_Global.IsAdmin(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	// Check if supplies are enabled
	static bool AreSuppliesEnabled(IEntity arsenalEntity)
	{
		if (!arsenalEntity)
			return false;
		
		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.Cast(SCR_EntityHelper.FindComponent(arsenalEntity, SCR_ResourceComponent));
		if (!resourceComponent)
			return false;
		
		return AreSuppliesEnabledForComponent(resourceComponent);
	}
	
	//------------------------------------------------------------------------------------------------
	static bool AreSuppliesEnabledForComponent(SCR_ResourceComponent resourceComponent)
	{
		if (!SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled())
			return false;

		if (!resourceComponent)
			return false;
		
		return resourceComponent.IsResourceTypeEnabled();
	}
	
	//------------------------------------------------------------------------------------------------
	// Check if file saving is possible on this platform
	static bool IsFileSavingEnabled()
	{
		if (System.GetPlatform() == EPlatform.WINDOWS || System.GetPlatform() == EPlatform.LINUX)
			return true;
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item preview manager
	static ItemPreviewManagerEntity GetPreviewManager()
	{
		if (!m_previewManager)
			m_previewManager = ChimeraWorld.CastFrom(GetGame().GetWorld()).GetItemPreviewManager();
		
		return m_previewManager;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get the player controller component
	static PQD_PlayerControllerComponent GetPlayerControllerComponent()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		return PQD_PlayerControllerComponent.Cast(pc.FindComponent(PQD_PlayerControllerComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	// Get faction from faction key
	static SCR_Faction GetFactionFromFactionKey(string factionKey)
	{
		FactionManager fm = GetGame().GetFactionManager();
		if (!fm)
		{
			Print("[PQD] No faction manager found", LogLevel.ERROR);
			return null;
		}
		
		SCR_Faction faction = SCR_Faction.Cast(fm.GetFactionByKey(factionKey));
		if (!faction)
		{
			Print(string.Format("[PQD] Invalid faction '%1'", factionKey), LogLevel.ERROR);
		}

		return faction;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get player identity ID
	static bool GetPlayerIdentityId(int playerId, out string identity)
	{
		BackendApi backend = GetGame().GetBackendApi();
		identity = backend.GetPlayerIdentityId(playerId);
		
		if (identity.IsEmpty())
		{
			if (GetGame().IsDev())
			{
				Print(string.Format("[PQD] Setting dev identity for player %1", playerId), LogLevel.DEBUG);
				identity = "dev_identity";
			}
			else
			{
				Print(string.Format("[PQD] No identity id for player %1", playerId), LogLevel.ERROR);
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get player entity faction key
	static bool GetPlayerEntityFactionKey(int playerId, out string factionKey)
	{
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlledEntity)
			return false;
		
		FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(controlledEntity.FindComponent(FactionAffiliationComponent));
		if (!factionComponent)
			return false;
		
		Faction faction = factionComponent.GetAffiliatedFaction();
		if (!faction || faction.GetFactionKey().IsEmpty())
			return false;
		
		factionKey = faction.GetFactionKey();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get ResourceName from entity
	static bool GetResourceNameFromEntity(IEntity ent, out ResourceName resourceName)
	{
		if (!ent)
			return false;

		EntityPrefabData entityPrefab = ent.GetPrefabData();
		if (!entityPrefab)
			return false;
			
		resourceName = entityPrefab.GetPrefabName();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item name from entity
	static string GetItemNameFromEntity(IEntity ent)
	{
		if (!ent)
			return "";

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(ent.FindComponent(InventoryItemComponent));
		if (!itemComponent)
			return "Unknown Item";
		
		UIInfo uiInfo = itemComponent.GetUIInfo();
		if (!uiInfo)
			return "Unknown Item";
		
		string name = uiInfo.GetName();
		if (name.IsEmpty())
			return "Item";
		
		return name;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item name from entity with validation
	static bool GetItemNameFromEntityValidate(IEntity ent, out string outItemName)
	{
		if (!ent)
			return false;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(ent.FindComponent(InventoryItemComponent));
		if (!itemComponent)
		{
			Print(string.Format("[PQD] Entity %1 has no InventoryItemComponent", ent), LogLevel.WARNING);
			return false;
		}
		
		UIInfo uiInfo = itemComponent.GetUIInfo();
		if (!uiInfo)
		{
			Print(string.Format("[PQD] Entity %1 has no UIInfo", ent), LogLevel.WARNING);
			return false;
		}
		
		outItemName = itemComponent.GetUIInfo().GetName();
		if (outItemName.IsEmpty())
			outItemName = "Item";
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item name from prefab resource name (without creating entity)
	static string GetItemNameFromPrefab(ResourceName prefab)
	{
		if (!prefab || prefab.IsEmpty())
			return "Unknown";
		
		// First try to resolve the prefab and get the name from UIInfo
		ItemPreviewManagerEntity previewManager = GetPreviewManager();
		if (previewManager)
		{
			IEntity previewEntity = previewManager.ResolvePreviewEntityForPrefab(prefab);
			if (previewEntity)
			{
				string name = GetItemNameFromEntity(previewEntity);
				SCR_EntityHelper.DeleteEntityAndChildren(previewEntity);
				if (!name.IsEmpty() && name != "Unknown Item")
					return name;
			}
		}
		
		// Fallback: extract name from prefab path
		// Path format: "{GUID}Prefabs/Items/Medicine/Bandage_01.et"
		string path = prefab;
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash >= 0 && lastSlash < path.Length() - 1)
		{
			string filename = path.Substring(lastSlash + 1, path.Length() - lastSlash - 1);
			// Remove .et extension
			int dotPos = filename.LastIndexOf(".");
			if (dotPos > 0)
				filename = filename.Substring(0, dotPos);
			// Replace underscores with spaces and clean up
			filename.Replace("_", " ");
			return filename;
		}
		
		return "Unknown Item";
	}
	
	//------------------------------------------------------------------------------------------------
	// Get inventory slot name
	static string GetInventorySlotName(InventoryStorageSlot slot)
	{
		BaseWeaponComponent weaponSlot = BaseWeaponComponent.Cast(slot.GetParentContainer());
		if (weaponSlot)
		{
			string slotType = weaponSlot.GetWeaponSlotType();
			string slotName = slotType.Substring(0, 1);
			slotName.ToUpper();
			return slotName + slotType.Substring(1, slotType.Length() - 1);
		}
		
		return slot.GetSourceName();
	}
	
	//------------------------------------------------------------------------------------------------
	// Get all entity storages
	static int GetAllEntityStorages(IEntity entity, out array<BaseInventoryStorageComponent> outComponents)
	{
		array<Managed> allComponents = {};
		
		int numStorages = entity.FindComponents(BaseInventoryStorageComponent, allComponents);
		if (numStorages < 1)
			return numStorages;
		
		int storageSlotCount;
		numStorages = 0;
		
		foreach (Managed component : allComponents)
		{
			BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(component);
			storageSlotCount = storageComponent.GetSlotsCount();
			
			if (storageSlotCount < 1)
				continue;
			
			outComponents.Insert(storageComponent);
			numStorages += 1;
		}
		
		return numStorages;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get all character item storage slots
	static int GetAllCharacterItemStorageSlots(IEntity entity, out array<InventoryStorageSlot> outSlots)
	{
		int numCharacterSlots;
		
		SCR_CharacterInventoryStorageComponent characterStorage = SCR_CharacterInventoryStorageComponent.Cast(entity.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!characterStorage)
			return 0;
		
		int numSlots = characterStorage.GetSlotsCount();
		if (numSlots < 1)
			return 0;
		
		for (int i = 0; i < numSlots; ++i)
		{
			InventoryStorageSlot clothingSlot = characterStorage.GetSlot(i);
			IEntity attached = clothingSlot.GetAttachedEntity();
			
			if (!attached)
				continue;
			
			BaseInventoryStorageComponent storageComp = BaseInventoryStorageComponent.Cast(attached.FindComponent(BaseInventoryStorageComponent));
			if (!storageComp)
				continue;
			
			if (!SCR_Enum.HasFlag(storageComp.GetPurpose(), EStoragePurpose.PURPOSE_DEPOSIT))
				continue;
			
			outSlots.Insert(clothingSlot);
			numCharacterSlots += 1;
		}
		
		return numCharacterSlots;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get storage type from component
	static PQD_StorageType GetStorageType(BaseInventoryStorageComponent comp)
	{
		if (SCR_CharacterInventoryStorageComponent.Cast(comp))
			return PQD_StorageType.CHARACTER_LOADOUT;
		
		if (SCR_WeaponAttachmentsStorageComponent.Cast(comp))
			return PQD_StorageType.WEAPON;
		
		if (EquipedWeaponStorageComponent.Cast(comp))
			return PQD_StorageType.CHARACTER_WEAPON;

		if (SCR_SalineStorageComponent.Cast(comp) || SCR_TourniquetStorageComponent.Cast(comp))
			return PQD_StorageType.IGNORED;

		if (SCR_EquipmentStorageComponent.Cast(comp))
			return PQD_StorageType.CHARACTER_EQUIPMENT;
		
		return PQD_StorageType.DEFAULT;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get loadout area type from slot
	static bool GetLoadoutAreaType(InventoryStorageSlot slot, out typename areaType)
	{
		LoadoutSlotInfo loadoutSlot = LoadoutSlotInfo.Cast(slot);
		if (!loadoutSlot)
			return false;
		
		LoadoutAreaType loadoutSlotAreaType = loadoutSlot.GetAreaType();
		if (!loadoutSlotAreaType)
			return false;
		
		areaType = loadoutSlotAreaType.Type();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get loadout slot area type string
	static string GetLoadoutSlotAreaTypeString(InventoryStorageSlot slot)
	{
		typename loadoutSlotAreaType;
		
		if (!GetLoadoutAreaType(slot, loadoutSlotAreaType))
			return "";
		
		return loadoutSlotAreaType.ToString();
	}
	
	//------------------------------------------------------------------------------------------------
	// Get weapon type string
	static bool GetWeaponSlotTypeString(BaseWeaponComponent weapon, out string outName)
	{
		string weaponType = weapon.GetWeaponSlotType();
		if (weaponType.IsEmpty())
			return false;
		
		weaponType.ToLower();
		outName = weaponType;
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get weapon type from weapon slot
	static string GetWeaponTypeStringFromWeaponSlot(BaseWeaponComponent weapon)
	{
		string slotType = weapon.GetWeaponSlotType();
		slotType.ToLower();
		return slotType;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get arsenal item types and modes for weapon slot
	static bool GetArsenalItemTypesAndModesForWeaponSlot(string slotType, out SCR_EArsenalItemMode itemModes, out SCR_EArsenalItemType itemTypes)
	{
		itemModes = SCR_EArsenalItemMode.WEAPON | SCR_EArsenalItemMode.WEAPON_VARIANTS;
		
		if (slotType == "primary")
		{
			itemTypes = SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.MACHINE_GUN | SCR_EArsenalItemType.ROCKET_LAUNCHER;
			return true;
		}
		
		if (slotType == "launcher")
		{
			itemTypes = SCR_EArsenalItemType.ROCKET_LAUNCHER;
			return true;
		}
		
		if (slotType == "secondary")
		{
			itemTypes = SCR_EArsenalItemType.PISTOL;
			return true;
		}
		
		itemModes = SCR_EArsenalItemMode.DEFAULT;
		
		if (slotType == "grenade")
		{
			itemTypes = SCR_EArsenalItemType.LETHAL_THROWABLE;
			return true;
		}
		
		if (slotType == "throwable")
		{
			itemTypes = SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.LETHAL_THROWABLE;
			return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Check if item in slot is editable
	static bool IsItemInSlotEditable(IEntity ent)
	{
		if (!ent)
		{
			Print("[PQD] IsItemInSlotEditable: entity is null", LogLevel.DEBUG);
			return false;
		}

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(ent.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
		{
			Print(string.Format("[PQD] IsItemInSlotEditable: %1 has no BaseInventoryStorageComponent", ent), LogLevel.DEBUG);
		 	return false;
		}
		
		if (SCR_UniversalInventoryStorageComponent.Cast(storage))
		{
			Print(string.Format("[PQD] IsItemInSlotEditable: %1 is SCR_UniversalInventoryStorageComponent (ignored)", ent), LogLevel.DEBUG);
			return false;
		}
		
		int slotCount = storage.GetSlotsCount();
		if (slotCount < 1)
		{
			Print(string.Format("[PQD] IsItemInSlotEditable: %1 has 0 slots", ent), LogLevel.DEBUG);
			return false;
		}
		
		Print(string.Format("[PQD] IsItemInSlotEditable: %1 IS EDITABLE (slots=%2)", ent, slotCount), LogLevel.NORMAL);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get slot type from slot
	static PQD_SlotType GetSlotTypeFromSlot(InventoryStorageSlot slot)
	{
		if (LoadoutSlotInfo.Cast(slot))
			return PQD_SlotType.CHARACTER_LOADOUT;
		
		if (AttachmentSlotComponent.Cast(slot.GetParentContainer()))
			return PQD_SlotType.ATTACHMENT;
		
		if (BaseWeaponComponent.Cast(slot.GetParentContainer()))
			return PQD_SlotType.CHARACTER_WEAPON;

		return PQD_SlotType.UNKNOWN;
	}
	
	//------------------------------------------------------------------------------------------------
	// Validate and fill slot info
	static bool ValidateAndFillSlotInfo(InventoryStorageSlot slot, out PQD_SlotInfo slotInfo)
	{
		slotInfo.slotName = "Invalid";
		slotInfo.itemName = "Unknown Slot Type";
		
		if (!slot)
		{
			slotInfo.slotName = "Invalid";
			slotInfo.itemName = "Invalid Slot: null";
			return false;
		}
		
		slotInfo.storageSlotId = slot.GetID();
		
		if (IsValidSlot_LoadoutSlotInfo(LoadoutSlotInfo.Cast(slot), slotInfo))
			return true;
		
		if (IsValidSlot_AttachmentSlot(AttachmentSlotComponent.Cast(slot.GetParentContainer()), slot, slotInfo))
			return true;
		
		if (IsValidSlot_CharacterWeaponSlot(BaseWeaponComponent.Cast(slot.GetParentContainer()), slotInfo))
			return true;
		
		if (IsValidSlot_CharacterEquipment(EquipmentStorageSlot.Cast(slot), slotInfo))
			return true;
		
		if (IsValidSlot_Magazine(BaseMuzzleComponent.Cast(slot.GetParentContainer()), slotInfo))
			return true;
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsValidSlot_LoadoutSlotInfo(LoadoutSlotInfo slot, out PQD_SlotInfo slotInfo)
	{
		if (!slot)
			return false;
				
		LoadoutAreaType loadoutSlotAreaType = slot.GetAreaType();
		if (!loadoutSlotAreaType)
		{
			slotInfo.itemName = "Invalid: No LoadoutAreaType";
			return false;
		}
		
		slotInfo.slotName = slot.GetSourceName();
		slotInfo.slotType = PQD_SlotType.CHARACTER_LOADOUT;

		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsValidSlot_AttachmentSlot(AttachmentSlotComponent slot, InventoryStorageSlot inventorySlot, out PQD_SlotInfo slotInfo)
	{
		if (!slot)
			return false;
		
		if (!slot.GetAttachmentSlotType())
		{
			slotInfo.itemName = "Invalid: No Attachment Slot Type";
			return false;
		}
		
		slotInfo.slotName = inventorySlot.GetSourceName();
		if (slotInfo.slotName.IsEmpty())
			slotInfo.slotName = "Attachment";
		
		slotInfo.slotType = PQD_SlotType.ATTACHMENT;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsValidSlot_CharacterWeaponSlot(BaseWeaponComponent weapon, out PQD_SlotInfo slotInfo)
	{
		if (!weapon)
			return false;

		string weaponType;
		if (!GetWeaponSlotTypeString(weapon, weaponType))
		{
			slotInfo.slotName = "Weapon";
			slotInfo.itemName = "Invalid: No Weapon Type";
			return false;
		}

		switch (weaponType)
		{
			case "primary":
				slotInfo.slotName = "Primary Weapon";
				break;
			case "launcher":
				slotInfo.slotName = "Launcher";
				break;
			case "secondary":
				slotInfo.slotName = "Secondary Weapon";
				break;
			case "grenade":
				slotInfo.slotName = "Grenade";
				break;
			case "throwable":
				slotInfo.slotName = "Throwable";
				break;
			default:
				slotInfo.slotName = "Weapon";
				slotInfo.itemName = "Invalid: Unknown Type";
				return false;
		}
		
		slotInfo.slotType = PQD_SlotType.CHARACTER_WEAPON;
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsValidSlot_CharacterEquipment(EquipmentStorageSlot slot, out PQD_SlotInfo slotInfo)
	{
		if (!slot)
			return false;
		
		slotInfo.slotName = slot.GetSourceName();
		slotInfo.slotType = PQD_SlotType.CHARACTER_EQUIPMENT;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool IsValidSlot_Magazine(BaseMuzzleComponent muzzle, out PQD_SlotInfo slotInfo)
	{
		if (!muzzle)
			return false;
		
		slotInfo.slotName = "Ammo";
		if (muzzle.IsDisposable())
		{
			slotInfo.itemName = "Disposable Weapon";
			return false;
		}
		
		slotInfo.itemName = "Invalid Slot: No Magazine Well";
		slotInfo.slotType = PQD_SlotType.MAGAZINE;
		
		BaseMagazineWell magWell = muzzle.GetMagazineWell();
		if (!magWell)
			return false;
		
		slotInfo.itemName = "";
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Prepare temporary entity at coordinates
	static IEntity PrepareTemporaryEntityAtCoords(ResourceName prefabResource, vector coords)
	{
		Resource loaded = Resource.Load(prefabResource);
		if (!loaded)
			return null;

		EntitySpawnParams params = new EntitySpawnParams;
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = coords;
		
		return GetGame().SpawnEntityPrefabEx(prefabResource, false, GetGame().GetWorld(), params);
	}
	
	//------------------------------------------------------------------------------------------------
	// Prepare temporary entity
	static IEntity PrepareTemporaryEntity(ResourceName prefabResource)
	{
		Resource loaded = Resource.Load(prefabResource);
		if (!loaded)
			return null;

		return GetGame().SpawnEntityPrefabEx(prefabResource, false, GetGame().GetWorld(), null);
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item supply cost
	static float GetItemSupplyCost(SCR_ArsenalComponent arsenalComponent, ResourceName prefab)
	{
		float resourceCost = 0;
		
		if (!arsenalComponent || !arsenalComponent.IsArsenalUsingSupplies())
			return resourceCost;
		
		PQD_ArsenalItemDetails details = m_ArsenalItemDetails.Get(prefab);
		if (details)
			return details.supplyCost;

		SCR_EntityCatalogManagerComponent entityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (entityCatalogManager)
		{
			SCR_Faction faction;
			if (arsenalComponent)
				faction = arsenalComponent.GetAssignedFaction();
			
			SCR_EntityCatalogEntry entry;
			
			if (faction)
				entry = entityCatalogManager.GetEntryWithPrefabFromFactionCatalog(EEntityCatalogType.ITEM, prefab, faction);
			else 
				entry = entityCatalogManager.GetEntryWithPrefabFromCatalog(EEntityCatalogType.ITEM, prefab);
			
			if (entry)
			{
				SCR_ArsenalItem data = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			
				if (data)
				{
					FillSupplyAndRankCache(data);

					if (arsenalComponent)
						resourceCost = data.GetSupplyCost(arsenalComponent.GetSupplyCostType());
					else 
						resourceCost = data.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT);
				}
			}
		}
		
		return resourceCost;
	}
	
	//------------------------------------------------------------------------------------------------
	// Fill supply and rank cache
	static void FillSupplyAndRankCache(SCR_ArsenalItem arsenalItem)
	{
		PQD_ArsenalItemDetails details = new PQD_ArsenalItemDetails;
		details.supplyCost = arsenalItem.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT);
		details.requiredRank = arsenalItem.GetRequiredRank();
				
		m_ArsenalItemDetails.Set(arsenalItem.GetItemResourceName(), details);
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item required rank from cache
	static SCR_ECharacterRank GetItemRequiredRankFromCache(ResourceName prefab)
	{
		PQD_ArsenalItemDetails details = m_ArsenalItemDetails.Get(prefab);
		if (details)
			return details.requiredRank;
		
		return SCR_ECharacterRank.INVALID;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get item required rank
	static SCR_ECharacterRank GetItemRequiredRank(SCR_ArsenalComponent arsenalComponent, ResourceName prefab)
	{
		SCR_ECharacterRank requiredRank = SCR_ECharacterRank.INVALID;
		
		if (!arsenalComponent)
			return requiredRank;
		
		PQD_ArsenalItemDetails details = m_ArsenalItemDetails.Get(prefab);
		if (details)
			return details.requiredRank;
		
		SCR_EntityCatalogManagerComponent entityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (entityCatalogManager)
		{
			SCR_Faction faction;
			if (arsenalComponent)
				faction = arsenalComponent.GetAssignedFaction();
			
			SCR_EntityCatalogEntry entry;
			
			if (faction)
				entry = entityCatalogManager.GetEntryWithPrefabFromFactionCatalog(EEntityCatalogType.ITEM, prefab, faction);
			else 
				entry = entityCatalogManager.GetEntryWithPrefabFromCatalog(EEntityCatalogType.ITEM, prefab);
			
			if (entry)
			{
				SCR_ArsenalItem data = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			
				if (data)
				{
					FillSupplyAndRankCache(data);
					requiredRank = data.GetRequiredRank();
				}
			}
		}
		
		return requiredRank;
	}
	
	//------------------------------------------------------------------------------------------------
	// Create dialog
	static SCR_ConfigurableDialogUi CreateDialog(string dialogType, string dialogContent, string debugInformation = "")
	{
		// Check if preset file exists
		if (!Resource.Load(m_sDialogPresets))
		{
			Print(string.Format("[PQD] Dialog preset file not found: %1", m_sDialogPresets), LogLevel.WARNING);
			Print(string.Format("[PQD] Dialog message: %1", dialogContent), LogLevel.NORMAL);
			return null;
		}
		
		m_dialog = SCR_ConfigurableDialogUi.CreateFromPreset(m_sDialogPresets, dialogType);
		if (!m_dialog)
		{
			Print(string.Format("[PQD] Failed to create dialog '%1' - preset not found", dialogType), LogLevel.WARNING);
			Print(string.Format("[PQD] Dialog message: %1", dialogContent), LogLevel.NORMAL);
			return null;
		}
		
		m_dialog.SetMessage(dialogContent);
		
		if (!debugInformation.IsEmpty())
		{
			RichTextWidget debugWidget = RichTextWidget.Cast(m_dialog.GetRootWidget().FindAnyWidget("DebugMessage"));
			if (debugWidget)
				debugWidget.SetText(debugInformation);
			
			Widget debugPanel = m_dialog.GetRootWidget().FindAnyWidget("DebugMessagePanel");
			if (debugPanel)
				debugPanel.SetVisible(true);
		}
		
		m_dialog.GetRootWidget().SetZOrder(1000);
		
		return m_dialog;
	}
	
	//------------------------------------------------------------------------------------------------
	// Play sound for slot type
	static void PlaySoundForSlotType(PQD_SlotType slotType)
	{
		switch (slotType)
		{
			case PQD_SlotType.CHARACTER_LOADOUT:
				AudioSystem.PlaySound("{B07341E25EBDF99F}Sounds/Items/_SharedData/Equip/Samples/Backpacks/Items_Equip_GenericBackpack_02.wav");
				break;
			case PQD_SlotType.CHARACTER_WEAPON:
				AudioSystem.PlaySound("{260F0D3FFD828137}Sounds/Weapons/_SharedData/PickUp/Samples/Weapons/Weapons_PickUp_Rifle_Plastic_01.wav");
				break;
			case PQD_SlotType.CHARACTER_EQUIPMENT:
				AudioSystem.PlaySound("{47D9797F7C0A4ECB}Sounds/Items/_SharedData/PickUp/Samples/Radio_BackPack/Items_PickUp_RadioBackpack_01.wav");
				break;
			case PQD_SlotType.ATTACHMENT:
				AudioSystem.PlaySound("{77825D9B98E1EED0}Sounds/Weapons/_SharedData/PickUp/Samples/Magazines/Weapons_PickUp_Magazine_Handgun_01.wav");
				break;
			case PQD_SlotType.MAGAZINE:
				AudioSystem.PlaySound("{41435EA7CB79EE9C}Sounds/Weapons/_SharedData/PickUp/Samples/Magazines/Weapons_PickUp_Magazine_Rifle_01.wav");
				break;
			case PQD_SlotType.OPTION:
				AudioSystem.PlaySound("{AC4149A069A91112}Sounds/UI/Samples/Menu/UI_Button_Confirm.wav");
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Play sound for slot
	static void PlaySoundForSlot(PQD_SlotInfo slot)
	{
		switch (slot.slotType)
		{
			case PQD_SlotType.CHARACTER_LOADOUT:
				string nameLower = slot.slotName;
				nameLower.ToLower();
				if (nameLower.Contains("velcro"))
				{
					SCR_UISoundEntity.SoundEvent("PQD_SOUND_VELCRO");
					break;
				}
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_LOADOUT");
				break;
			case PQD_SlotType.CHARACTER_WEAPON:
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_WEAPON");
				break;
			case PQD_SlotType.CHARACTER_EQUIPMENT:
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_EQUIPMENT");
				break;
			case PQD_SlotType.ATTACHMENT:
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_ATTACHMENT");
				break;
			case PQD_SlotType.MAGAZINE:
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_MAGAZINE");
				break;
			case PQD_SlotType.OPTION:
				SCR_UISoundEntity.SoundEvent("PQD_SOUND_OPTION");
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Play blocked sound
	static void PlaySound(string type)
	{
		switch (type)
		{
			case "blocked":
				SCR_UISoundEntity.SoundEvent("SOUND_HUD_ACTION_FAILED");
				break;
			case "SoundUI_Select":
				SCR_UISoundEntity.SoundEvent("SOUND_E_FOCUS_WIDGET");
				break;
			default:
				SCR_UISoundEntity.SoundEvent("SOUND_HUD_POPUP");
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Find AI Agent
	static SCR_ChimeraAIAgent FindAIAgent(IEntity entity)
	{
		if (entity)
		{
			AIControlComponent controlComponent = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
			if (controlComponent)
			{
				SCR_ChimeraAIAgent agent = SCR_ChimeraAIAgent.Cast(controlComponent.GetControlAIAgent());
				return agent;
			}
		}
		
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get character equipped weapon magazine wells
	static int GetCharacterEquippedWeaponMagazineWells(IEntity characterEntity, out set<string> magWells)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(characterEntity);
		if (!character)
			return 0;
		
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return 0;
		
		BaseWeaponManagerComponent weaponManager = controller.GetWeaponManagerComponent();
		if (!weaponManager)
			return 0;
		
		array<IEntity> weapons = {};
		weaponManager.GetWeaponsList(weapons);
		
		int count = 0;
		
		BaseMuzzleComponent muzzle;
		BaseMagazineWell magwell;
		
		foreach (IEntity weapon : weapons)
		{
			muzzle = BaseMuzzleComponent.Cast(weapon.FindComponent(BaseMuzzleComponent));
			if (!muzzle)
				continue;
			
			magwell = muzzle.GetMagazineWell();
			if (!magwell)
				continue;
			
			magWells.Insert(muzzle.GetMagazineWell().Type().ToString());
			count += 1;
		}
		
		return count;
	}
}
