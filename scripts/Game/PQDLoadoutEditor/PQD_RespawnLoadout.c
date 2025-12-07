// PQD Loadout Editor - Respawn Loadout Integration
// Author: PQD Team
// Version: 1.0.0
// Description: Component for managing PQD loadout system configuration
// Note: The actual loadout logic is in PQD_ModdedPlayerArsenalLoadout.c and PQD_ModdedLoadoutManager.c

//------------------------------------------------------------------------------------------------
//! Component class for PQD respawn loadout system
sealed class PQD_RespawnLoadoutComponentClass : SCR_BaseGameModeComponentClass {}

//------------------------------------------------------------------------------------------------
//! Component that manages PQD loadouts in the respawn/deploy menu
//! Attach this to the GameMode to enable custom loadouts in respawn
sealed class PQD_RespawnLoadoutComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable PQD custom loadouts in respawn menu", category: "PQD Loadout Editor")]
	protected bool m_bEnabled;
	
	[Attribute("5", UIWidgets.Slider, "Maximum number of custom loadouts to show (1-5)", "1 5 1", category: "PQD Loadout Editor")]
	protected int m_iMaxLoadoutsToShow;
	
	[Attribute("0", UIWidgets.CheckBox, "Show empty loadout slots in the menu", category: "PQD Loadout Editor")]
	protected bool m_bShowEmptySlots;
	
	// Static instance for easy access
	protected static PQD_RespawnLoadoutComponent s_Instance;
	
	//------------------------------------------------------------------------------------------------
	//! Get the singleton instance
	static PQD_RespawnLoadoutComponent GetInstance()
	{
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if the system is enabled
	bool IsEnabled()
	{
		return m_bEnabled;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get maximum loadouts to show
	int GetMaxLoadoutsToShow()
	{
		return m_iMaxLoadoutsToShow;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if empty slots should be shown
	bool ShouldShowEmptySlots()
	{
		return m_bShowEmptySlots;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		s_Instance = this;
		
		if (m_bEnabled)
			Print("[PQD] Respawn Loadout Component initialized - custom loadouts enabled in deploy menu", LogLevel.NORMAL);
		else
			Print("[PQD] Respawn Loadout Component initialized - custom loadouts DISABLED", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_Instance = null;
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! Modded SCR_BaseGameMode - handles fallback loadout application after player spawns
modded class SCR_BaseGameMode : BaseGameMode
{
	// Max retry attempts for fallback loadout application
	protected static const int PQD_MAX_RETRY_ATTEMPTS = 3;
	// Delay between retry attempts in milliseconds
	protected static const int PQD_RETRY_DELAY_MS = 1000;
	// Initial delay before first fallback attempt
	protected static const int PQD_INITIAL_DELAY_MS = 500;
	
	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		
		Print(string.Format("[PQD] OnPlayerSpawned: Player %1 spawned, checking for PQD selection...", playerId), LogLevel.NORMAL);
		
		// First, check if there's a pending loadout already (from OnLoadoutSpawned or pre-cached RPC)
		if (PQD_PendingLoadoutManager.HasPendingLoadout(playerId))
		{
			PQD_PendingLoadout pending = PQD_PendingLoadoutManager.GetPendingLoadout(playerId);
			
			// Verify the pending loadout actually has valid data
			if (pending && !pending.loadoutData.IsEmpty())
			{
				Print(string.Format("[PQD] Fallback: Player %1 has valid pending loadout with %2 bytes of data", 
					playerId, pending.loadoutData.Length()), LogLevel.NORMAL);
				GetGame().GetCallqueue().CallLater(PQD_ApplyFallbackLoadout, PQD_INITIAL_DELAY_MS, false, playerId, controlledEntity);
				return;
			}
			else
			{
				Print(string.Format("[PQD] Fallback: Player %1 has pending loadout but NO DATA - will try server selection", playerId), LogLevel.WARNING);
				// Remove the empty pending loadout
				PQD_PendingLoadoutManager.RemovePendingLoadout(playerId);
				// Fall through to server selection check below
			}
		}
		
		// Check if there's a server-side PQD selection (this is the fallback mechanism)
		// This handles the case where OnLoadoutSpawned wasn't called or data wasn't available
		PQD_PendingLoadout serverSelection = PQD_ServerLoadoutSelection.GetPlayerSelection(playerId);
		if (serverSelection)
		{
			Print(string.Format("[PQD] Fallback: Player %1 has server-side PQD selection for slot %2, faction %3", 
				playerId, serverSelection.slotId, serverSelection.factionKey), LogLevel.NORMAL);
			
			// Get the loadout data from storage
			PQD_LoadoutStorageComponent storageComp = PQD_LoadoutStorageComponent.Cast(FindComponent(PQD_LoadoutStorageComponent));
			if (storageComp)
			{
				// Get slot index from slot ID
				int slotIndex = PQD_GetSlotIndex(serverSelection.slotId);
				
				if (slotIndex >= 0)
				{
					string prefab, loadoutData, requiredRank;
					float cost;
					
					if (storageComp.GetPlayerLoadoutData(playerId, serverSelection.factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank))
					{
						Print(string.Format("[PQD] Fallback: Found loadout data for player %1 (size: %2 bytes), scheduling replacement", 
							playerId, loadoutData.Length()), LogLevel.NORMAL);
						
						// Add to pending loadout manager
						PQD_PendingLoadoutManager.AddPendingLoadout(playerId, serverSelection.slotId, loadoutData, serverSelection.factionKey, prefab);
						
						// Clear the server selection
						PQD_ServerLoadoutSelection.ClearPlayerSelection(playerId);
						
						// Schedule the fallback application
						GetGame().GetCallqueue().CallLater(PQD_ApplyFallbackLoadout, PQD_INITIAL_DELAY_MS, false, playerId, controlledEntity);
						return;
					}
					else
					{
						Print(string.Format("[PQD] Fallback: FAILED to get loadout data for player %1, slot %2 - PLAYER WILL SPAWN NAKED!", 
							playerId, serverSelection.slotId), LogLevel.ERROR);
					}
				}
				else
				{
					Print(string.Format("[PQD] Fallback: Invalid slot ID '%1'", serverSelection.slotId), LogLevel.ERROR);
				}
			}
			else
			{
				Print("[PQD] Fallback: PQD_LoadoutStorageComponent not found on GameMode!", LogLevel.ERROR);
			}
			
			// Clear the selection even if we couldn't get data
			PQD_ServerLoadoutSelection.ClearPlayerSelection(playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Replace player entity with a new one that has the correct loadout
	protected void PQD_ApplyFallbackLoadout(int playerId, IEntity controlledEntity)
	{
		// Get the pending loadout
		PQD_PendingLoadout pending = PQD_PendingLoadoutManager.GetPendingLoadout(playerId);
		if (!pending)
		{
			Print(string.Format("[PQD] Fallback: No pending loadout found for player %1", playerId), LogLevel.DEBUG);
			return;
		}
		
		// Validate controlled entity
		GameEntity playerEntity = GameEntity.Cast(controlledEntity);
		if (!playerEntity)
		{
			Print(string.Format("[PQD] Fallback: Player entity is null for player %1", playerId), LogLevel.ERROR);
			PQD_PendingLoadoutManager.RemovePendingLoadout(playerId);
			return;
		}
		
		// Check if entity is still valid (player might have died or disconnected)
		IEntity currentEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (currentEntity != controlledEntity)
		{
			Print(string.Format("[PQD] Fallback: Controlled entity changed for player %1, aborting", playerId), LogLevel.DEBUG);
			PQD_PendingLoadoutManager.RemovePendingLoadout(playerId);
			return;
		}
		
		pending.retryCount++;
		
		Print(string.Format("[PQD] Fallback: Replacing entity for player %1 (attempt %2/%3)", 
			playerId, pending.retryCount, PQD_MAX_RETRY_ATTEMPTS), LogLevel.NORMAL);
		
		// Replace the entity with a new one that has the correct loadout
		if (PQD_ReplaceEntityWithLoadout(playerId, playerEntity, pending.prefab, pending.loadoutData, pending.factionKey))
		{
			Print(string.Format("[PQD] Fallback: Successfully replaced entity for player %1 with loadout from slot %2", 
				playerId, pending.slotId), LogLevel.NORMAL);
			PQD_PendingLoadoutManager.RemovePendingLoadout(playerId);
			return;
		}
		
		// Check if we should retry
		if (pending.retryCount < PQD_MAX_RETRY_ATTEMPTS)
		{
			Print(string.Format("[PQD] Fallback: Failed to replace entity, will retry in %1ms", PQD_RETRY_DELAY_MS), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(PQD_ApplyFallbackLoadout, PQD_RETRY_DELAY_MS, false, playerId, controlledEntity);
		}
		else
		{
			Print(string.Format("[PQD] Fallback: Failed to replace entity after %1 attempts, giving up", PQD_MAX_RETRY_ATTEMPTS), LogLevel.ERROR);
			PQD_PendingLoadoutManager.RemovePendingLoadout(playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Replace entity with a new one with the correct loadout (similar to arsenal approach)
	protected bool PQD_ReplaceEntityWithLoadout(int playerId, GameEntity oldEntity, string prefab, string loadoutData, string factionKey)
	{
		if (prefab.IsEmpty())
		{
			Print("[PQD] Fallback: Character prefab is empty", LogLevel.ERROR);
			return false;
		}
		
		if (loadoutData.IsEmpty())
		{
			Print("[PQD] Fallback: Loadout data is empty", LogLevel.ERROR);
			return false;
		}
		
		// Get spawn transform from old entity
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		oldEntity.GetWorldTransform(params.Transform);
		params.Transform[3] = params.Transform[3] + (vector.Up * 0.05);
		
		// Load prefab resource
		Resource loaded = Resource.Load(prefab);
		if (!loaded)
		{
			Print(string.Format("[PQD] Fallback: Failed to load character prefab: %1", prefab), LogLevel.ERROR);
			return false;
		}
		
		// Spawn new entity
		IEntity newEntity = GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), params);
		if (!newEntity)
		{
			Print("[PQD] Fallback: Failed to spawn new character entity", LogLevel.ERROR);
			return false;
		}
		
		GameEntity newGameEntity = GameEntity.Cast(newEntity);
		if (!newGameEntity)
		{
			Print("[PQD] Fallback: Spawned entity is not a GameEntity", LogLevel.ERROR);
			RplComponent.DeleteRplEntity(newEntity, false);
			return false;
		}
		
		// Set player pending on AI agent
		SCR_ChimeraAIAgent agent = SCR_ChimeraAIAgent.Cast(newGameEntity.FindComponent(SCR_ChimeraAIAgent));
		if (agent)
			agent.SetPlayerPending_S(playerId);
		
		// Set player pending on editable character
		SCR_EditableCharacterComponent editorCharacter = SCR_EditableCharacterComponent.Cast(newGameEntity.FindComponent(SCR_EditableCharacterComponent));
		if (editorCharacter)
			editorCharacter.SetIsPlayerPending(playerId);
		
		// Verify character controller exists
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(newGameEntity.FindComponent(CharacterControllerComponent));
		if (!controller)
		{
			Print("[PQD] Fallback: New entity has no Character Controller", LogLevel.ERROR);
			RplComponent.DeleteRplEntity(newEntity, false);
			return false;
		}
		
		// Unequip right hand to prepare for loadout application
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
		
		Print(string.Format("[PQD] Fallback: Spawned new entity, applying loadout data (size: %1 bytes)", loadoutData.Length()), LogLevel.NORMAL);
		
		// CRITICAL: Ensure ARSENALLOADOUT_COMPONENTS_TO_CHECK is initialized before calling ApplyLoadoutString
		// This array tells the vanilla system which storage components to process for items
		// Without it, only equipped items are applied, not inventory items inside storages!
		if (!SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK || SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK.IsEmpty())
		{
			Print("[PQD] Fallback: Initializing ARSENALLOADOUT_COMPONENTS_TO_CHECK (was not initialized!)", LogLevel.WARNING);
			SCR_ArsenalManagerComponent.GetArsenalLoadoutComponentsToCheck(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK);
		}
		
		// Parse and apply loadout data
		SCR_JsonLoadContext context = new SCR_JsonLoadContext();
		if (!context.ImportFromString(loadoutData))
		{
			Print("[PQD] Fallback: Failed to parse loadout JSON", LogLevel.ERROR);
			RplComponent.DeleteRplEntity(newEntity, false);
			return false;
		}
		
		// Apply loadout to the NEW entity (this should work because it's a fresh entity)
		if (!SCR_PlayerArsenalLoadout.ApplyLoadoutString(newGameEntity, context))
		{
			Print("[PQD] Fallback: ApplyLoadoutString failed on new entity", LogLevel.ERROR);
			RplComponent.DeleteRplEntity(newEntity, false);
			return false;
		}
		
		Print("[PQD] Fallback: Loadout applied to new entity successfully", LogLevel.NORMAL);
		
		// VERIFY the new entity actually has items before proceeding
		// Use item count as a proxy for item verification
		SCR_InventoryStorageManagerComponent invMgr = SCR_InventoryStorageManagerComponent.Cast(newEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (invMgr)
		{
			array<IEntity> items = {};
			invMgr.GetItems(items);
			int itemCount = items.Count();
			if (itemCount < 2) // At minimum should have some items (clothing, etc)
			{
				Print(string.Format("[PQD] Fallback: WARNING - New entity only has %1 items, loadout may have partially failed!", itemCount), LogLevel.WARNING);
			}
			else
			{
				Print(string.Format("[PQD] Fallback: New entity has %1 items - loadout verified OK", itemCount), LogLevel.NORMAL);
			}
		}
		
		// Copy rank from old entity to new entity
		SCR_ECharacterRank rank = SCR_CharacterRankComponent.GetCharacterRank(oldEntity);
		if (rank != SCR_ECharacterRank.INVALID)
		{
			SCR_CharacterRankComponent rankComp = SCR_CharacterRankComponent.Cast(newEntity.FindComponent(SCR_CharacterRankComponent));
			if (rankComp)
				rankComp.SetCharacterRank(rank, true);
		}
		
		// Transfer control to new entity
		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (pc)
		{
			SCR_PlayerController scrPC = SCR_PlayerController.Cast(pc);
			if (scrPC)
				scrPC.SetInitialMainEntity(newEntity);
		}
		
		// Finalize spawn callbacks
		SCR_PerceivedFactionManagerComponent perceivedFactionManager = SCR_PerceivedFactionManagerComponent.GetInstance();
		if (perceivedFactionManager)
			perceivedFactionManager.OnPlayerSpawnFinalize_S(null, null, null, newEntity);
		
		// NOTE: We intentionally do NOT call OnPlayerSpawned here to avoid recursion
		// The player's loadout is already applied, so no need for the fallback system to trigger again
		
		Print("[PQD] Fallback: Deleting old entity, player now controls new entity", LogLevel.NORMAL);
		
		// Delete old entity
		RplComponent.DeleteRplEntity(oldEntity, false);
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Apply serialized loadout data to a character entity (fallback version - DEPRECATED)
	protected bool PQD_ApplyLoadoutFallback(GameEntity playerEntity, string loadoutData, string factionKey)
	{
		if (loadoutData.IsEmpty())
		{
			Print("[PQD] Fallback: Loadout data is empty", LogLevel.WARNING);
			return false;
		}
		
		// Create JSON load context
		SCR_JsonLoadContext context = new SCR_JsonLoadContext();
		
		if (!context.ImportFromString(loadoutData))
		{
			Print("[PQD] Fallback: Failed to parse loadout data", LogLevel.ERROR);
			return false;
		}
		
		// CRITICAL: Ensure ARSENALLOADOUT_COMPONENTS_TO_CHECK is initialized
		if (!SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK || SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK.IsEmpty())
		{
			Print("[PQD] DeprecatedFallback: Initializing ARSENALLOADOUT_COMPONENTS_TO_CHECK", LogLevel.WARNING);
			SCR_ArsenalManagerComponent.GetArsenalLoadoutComponentsToCheck(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK);
		}
		
		// Try to apply using the vanilla system
		if (!SCR_PlayerArsenalLoadout.ApplyLoadoutString(playerEntity, context))
		{
			Print("[PQD] Fallback: ApplyLoadoutString failed", LogLevel.ERROR);
			return false;
		}
		
		Print("[PQD] Fallback: Loadout applied successfully via ApplyLoadoutString", LogLevel.NORMAL);
		return true;
	}
}
