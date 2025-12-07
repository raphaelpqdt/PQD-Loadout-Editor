// PQD Loadout Editor - Modded Player Arsenal Loadout
// Author: PQD Team
// Version: 1.1.0
// Description: Extends SCR_PlayerArsenalLoadout to support PQD saved loadouts in deploy menu
// Fix: Added client-side cache for loadout slot validity AND loadout data to work on dedicated servers

//------------------------------------------------------------------------------------------------
//! Cached loadout data for a single slot
class PQD_CachedLoadoutData
{
	string prefab;
	string loadoutData;
	float cost;
	string requiredRank;
	
	void PQD_CachedLoadoutData(string _prefab, string _loadoutData, float _cost, string _requiredRank)
	{
		prefab = _prefab;
		loadoutData = _loadoutData;
		cost = _cost;
		requiredRank = _requiredRank;
	}
}

//------------------------------------------------------------------------------------------------
//! Static cache for valid loadout slots AND loadout data on the client-side
//! This is populated by the server via RPC when player connects or saves loadouts
class PQD_ClientLoadoutCache
{
	// Map: factionKey -> array of valid slot indices (0-4)
	protected static ref map<string, ref array<int>> s_mValidSlots = new map<string, ref array<int>>();
	
	// Map: "factionKey_slotIndex" -> cached loadout data
	protected static ref map<string, ref PQD_CachedLoadoutData> s_mLoadoutData = new map<string, ref PQD_CachedLoadoutData>();
	
	// Flag to track if cache has been initialized from server
	protected static bool s_bCacheInitialized = false;
	
	//------------------------------------------------------------------------------------------------
	//! Get the key for loadout data map
	protected static string GetLoadoutKey(string factionKey, int slotIndex)
	{
		return string.Format("%1_%2", factionKey, slotIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if a specific slot is valid for a faction
	static bool IsSlotValid(string factionKey, int slotIndex)
	{
		// If cache not initialized yet, show all slots to allow user to see them
		// The actual validation will happen when they try to spawn
		if (!s_bCacheInitialized)
		{
			Print(string.Format("[PQD] ClientCache: Not initialized yet, showing slot %1 for faction %2", slotIndex, factionKey), LogLevel.DEBUG);
			return true; // Show slots until we get server data
		}
		
		if (!s_mValidSlots.Contains(factionKey))
		{
			Print(string.Format("[PQD] ClientCache: No data for faction %1", factionKey), LogLevel.DEBUG);
			return false;
		}
		
		array<int> validSlots = s_mValidSlots.Get(factionKey);
		bool isValid = validSlots.Contains(slotIndex);
		
		string validStr;
		if (isValid)
			validStr = "valid";
		else
			validStr = "invalid";
		
		Print(string.Format("[PQD] ClientCache: Slot %1 for faction %2 is %3", slotIndex, factionKey, validStr), LogLevel.DEBUG);
		return isValid;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Update the cache with server data
	static void UpdateCache(string factionKey, array<int> validSlots)
	{
		s_bCacheInitialized = true;
		
		// Create a copy to avoid reference issues
		ref array<int> slotsCopy = new array<int>();
		
		if (validSlots)
		{
			foreach (int slot : validSlots)
				slotsCopy.Insert(slot);
		}
		
		s_mValidSlots.Set(factionKey, slotsCopy);
		
		Print(string.Format("[PQD] ClientCache: Updated faction %1 with %2 valid slots", factionKey, slotsCopy.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Store loadout data for a slot
	static void SetLoadoutData(string factionKey, int slotIndex, string prefab, string loadoutData, float cost, string requiredRank)
	{
		string key = GetLoadoutKey(factionKey, slotIndex);
		ref PQD_CachedLoadoutData cached = new PQD_CachedLoadoutData(prefab, loadoutData, cost, requiredRank);
		s_mLoadoutData.Set(key, cached);
		
		Print(string.Format("[PQD] ClientCache: Stored loadout data for %1 slot %2 (data size: %3)", factionKey, slotIndex, loadoutData.Length()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get cached loadout data for a slot
	static bool GetLoadoutData(string factionKey, int slotIndex, out string prefab, out string loadoutData, out float cost, out string requiredRank)
	{
		string key = GetLoadoutKey(factionKey, slotIndex);
		
		if (!s_mLoadoutData.Contains(key))
		{
			Print(string.Format("[PQD] ClientCache: No loadout data for %1 slot %2", factionKey, slotIndex), LogLevel.DEBUG);
			return false;
		}
		
		PQD_CachedLoadoutData cached = s_mLoadoutData.Get(key);
		prefab = cached.prefab;
		loadoutData = cached.loadoutData;
		cost = cached.cost;
		requiredRank = cached.requiredRank;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if we have cached loadout data for a slot
	static bool HasLoadoutData(string factionKey, int slotIndex)
	{
		string key = GetLoadoutKey(factionKey, slotIndex);
		return s_mLoadoutData.Contains(key);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clear loadout data for a slot
	static void ClearLoadoutData(string factionKey, int slotIndex)
	{
		string key = GetLoadoutKey(factionKey, slotIndex);
		s_mLoadoutData.Remove(key);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clear the cache (called on disconnect)
	static void ClearCache()
	{
		s_mValidSlots.Clear();
		s_mLoadoutData.Clear();
		s_bCacheInitialized = false;
		Print("[PQD] ClientCache: Cleared", LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Mark cache as initialized (even if empty)
	static void MarkInitialized()
	{
		s_bCacheInitialized = true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if cache is initialized
	static bool IsInitialized()
	{
		return s_bCacheInitialized;
	}
}

//------------------------------------------------------------------------------------------------
//! Class to store pending loadout data for fallback application
class PQD_PendingLoadout
{
	int playerId;
	string slotId;
	string loadoutData;
	string factionKey;
	string prefab;
	int retryCount;
	
	void PQD_PendingLoadout(int _playerId, string _slotId, string _loadoutData, string _factionKey, string _prefab = "")
	{
		playerId = _playerId;
		slotId = _slotId;
		loadoutData = _loadoutData;
		factionKey = _factionKey;
		prefab = _prefab;
		retryCount = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! Static manager for pending loadouts
class PQD_PendingLoadoutManager
{
	protected static ref map<int, ref PQD_PendingLoadout> s_mPendingLoadouts = new map<int, ref PQD_PendingLoadout>();
	
	//------------------------------------------------------------------------------------------------
	static void AddPendingLoadout(int playerId, string slotId, string loadoutData, string factionKey, string prefab = "")
	{
		PQD_PendingLoadout pending = new PQD_PendingLoadout(playerId, slotId, loadoutData, factionKey, prefab);
		s_mPendingLoadouts.Set(playerId, pending);
		Print(string.Format("[PQD] Added pending loadout for player %1, slot %2 (entity replacement mode)", playerId, slotId), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	static PQD_PendingLoadout GetPendingLoadout(int playerId)
	{
		PQD_PendingLoadout pending;
		if (s_mPendingLoadouts.Find(playerId, pending))
			return pending;
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	static void RemovePendingLoadout(int playerId)
	{
		s_mPendingLoadouts.Remove(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	static bool HasPendingLoadout(int playerId)
	{
		return s_mPendingLoadouts.Contains(playerId);
	}
}

//------------------------------------------------------------------------------------------------
//! Static class to track pending PQD loadout selections on the server
//! This is used because OnLoadoutSpawned may not be called for dynamically created PQD loadouts
class PQD_ServerLoadoutSelection
{
	// Map: playerId -> PQD selection data
	protected static ref map<int, ref PQD_PendingLoadout> s_mPendingSelections = new map<int, ref PQD_PendingLoadout>();
	
	//------------------------------------------------------------------------------------------------
	static void SetPlayerSelection(int playerId, string slotId, string factionKey)
	{
		// Store the selection - the loadout data will be fetched when spawning
		ref PQD_PendingLoadout pending = new PQD_PendingLoadout(playerId, slotId, "", factionKey, "");
		s_mPendingSelections.Set(playerId, pending);
		Print(string.Format("[PQD] ServerSelection: Stored selection for player %1, slot %2, faction %3", 
			playerId, slotId, factionKey), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	static PQD_PendingLoadout GetPlayerSelection(int playerId)
	{
		PQD_PendingLoadout pending;
		if (s_mPendingSelections.Find(playerId, pending))
			return pending;
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	static void ClearPlayerSelection(int playerId)
	{
		s_mPendingSelections.Remove(playerId);
		Print(string.Format("[PQD] ServerSelection: Cleared selection for player %1", playerId), LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	static bool HasPlayerSelection(int playerId)
	{
		return s_mPendingSelections.Contains(playerId);
	}
}

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sLoadoutName")]
modded class SCR_PlayerArsenalLoadout : SCR_FactionPlayerLoadout
{	
	[Attribute("")]
	string m_sPQDSlotId;
	
	//------------------------------------------------------------------------------------------------
	//! Override: Called when this loadout is spawned
	//! Applies the PQD saved loadout data if this is a PQD slot
	override void OnLoadoutSpawned(GenericEntity pOwner, int playerId)
	{
		// Set faction affiliation first
		if (pOwner)
		{
			FactionAffiliationComponent comp = FactionAffiliationComponent.Cast(pOwner.FindComponent(FactionAffiliationComponent));
			if (comp)
				comp.SetAffiliatedFactionByKey(m_sAffiliatedFaction);
		}
		
		// If this is not a PQD loadout slot, use default behavior
		if (m_sPQDSlotId.IsEmpty())
		{
			super.OnLoadoutSpawned(pOwner, playerId);
			return;
		}
		
		Print(string.Format("[PQD] OnLoadoutSpawned: Processing PQD slot %1 for player %2", m_sPQDSlotId, playerId), LogLevel.NORMAL);
		
		// Check if we already have pre-cached loadout from RPC (pre-cached in Rpc_NotifyPQDSelection_S)
		// This prevents race conditions where RPC arrived before OnLoadoutSpawned
		if (PQD_PendingLoadoutManager.HasPendingLoadout(playerId))
		{
			PQD_PendingLoadout pending = PQD_PendingLoadoutManager.GetPendingLoadout(playerId);
			if (pending && !pending.loadoutData.IsEmpty())
			{
				Print(string.Format("[PQD] OnLoadoutSpawned: Player %1 already has pre-cached pending loadout with %2 bytes of data", 
					playerId, pending.loadoutData.Length()), LogLevel.NORMAL);
				return; // Data already cached, OnPlayerSpawned fallback will handle it
			}
		}
		
		// This is a PQD loadout slot - try to get the data now
		GameEntity playerEntity = GameEntity.Cast(pOwner);
		if (!playerEntity)
		{
			Print("[PQD] OnLoadoutSpawned: Player entity is null", LogLevel.ERROR);
			return;
		}
		
		// Get the storage component from GameMode
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("[PQD] OnLoadoutSpawned: GameMode not found", LogLevel.ERROR);
			return;
		}
		
		PQD_LoadoutStorageComponent storageComp = PQD_LoadoutStorageComponent.Cast(gameMode.FindComponent(PQD_LoadoutStorageComponent));
		if (!storageComp)
		{
			Print("[PQD] OnLoadoutSpawned: PQD_LoadoutStorageComponent not found", LogLevel.WARNING);
			return;
		}
		
		// Get the player's faction
		string factionKey = m_sAffiliatedFaction;
		
		// Get the slot index from the slot ID (Slot1 -> 0, Slot2 -> 1, etc.)
		int slotIndex = PQD_GetSlotIndex(m_sPQDSlotId);
		if (slotIndex < 0)
		{
			Print(string.Format("[PQD] OnLoadoutSpawned: Invalid slot ID %1", m_sPQDSlotId), LogLevel.ERROR);
			return;
		}
		
		// Get the player UID
		string playerUID = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		
		// Get the loadout data
		string prefab, loadoutData, requiredRank;
		float cost;
		
		if (!storageComp.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank))
		{
			Print(string.Format("[PQD] OnLoadoutSpawned: No loadout data found for slot %1 - player may spawn naked!", m_sPQDSlotId), LogLevel.WARNING);
			// Still store the selection so fallback can try to load from file
			PQD_ServerLoadoutSelection.SetPlayerSelection(playerId, m_sPQDSlotId, factionKey);
			return;
		}
		
		// Don't try to apply loadout to the spawned entity - it doesn't work reliably
		// Instead, schedule a complete entity replacement after spawn
		// This is the same approach used successfully in the arsenal
		Print(string.Format("[PQD] OnLoadoutSpawned: Scheduling entity replacement for slot %1 (data size: %2)", m_sPQDSlotId, loadoutData.Length()), LogLevel.NORMAL);
		
		// Add to pending loadouts - the fallback will replace the entire entity
		PQD_PendingLoadoutManager.AddPendingLoadout(playerId, m_sPQDSlotId, loadoutData, factionKey, prefab);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override: Check if the loadout is available on client
	//! For PQD loadouts, this checks if the player has saved data for this slot
	override bool IsLoadoutAvailableClient()
	{
		// If not a PQD slot, use default behavior
		if (m_sPQDSlotId.IsEmpty())
			return super.IsLoadoutAvailableClient();
		
		// Check if player has saved loadout data for this slot
		bool isValid = PQD_IsPlayerLoadoutValid(m_sPQDSlotId);
		
		Print(string.Format("[PQD] IsLoadoutAvailableClient: slot %1, faction %2, valid=%3", m_sPQDSlotId, m_sAffiliatedFaction, isValid), LogLevel.DEBUG);
		
		return isValid;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if the player has a valid saved loadout for this slot
	protected bool PQD_IsPlayerLoadoutValid(string slotId)
	{
		// Get the storage component from GameMode
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("[PQD] PQD_IsPlayerLoadoutValid: GameMode not found", LogLevel.DEBUG);
			return false;
		}
		
		// Check if we should show empty slots (config option)
		PQD_RespawnLoadoutComponent respawnComp = PQD_RespawnLoadoutComponent.Cast(gameMode.FindComponent(PQD_RespawnLoadoutComponent));
		if (respawnComp && respawnComp.ShouldShowEmptySlots())
		{
			Print(string.Format("[PQD] PQD_IsPlayerLoadoutValid: ShowEmptySlots enabled, showing slot %1", slotId), LogLevel.DEBUG);
			return true;
		}
		
		// Get the slot index
		int slotIndex = PQD_GetSlotIndex(slotId);
		if (slotIndex < 0)
			return false;
		
		// Get faction key from this loadout
		string factionKey = m_sAffiliatedFaction;
		if (factionKey.IsEmpty())
		{
			Print("[PQD] PQD_IsPlayerLoadoutValid: Loadout has no affiliated faction", LogLevel.DEBUG);
			return false;
		}
		
		// Check if we're on the server or client
		RplComponent rpl = RplComponent.Cast(gameMode.FindComponent(RplComponent));
		bool isServer = !rpl || rpl.IsMaster();
		
		if (isServer)
		{
			// On server: check the actual storage component
			PQD_LoadoutStorageComponent storageComp = PQD_LoadoutStorageComponent.Cast(gameMode.FindComponent(PQD_LoadoutStorageComponent));
			if (!storageComp)
			{
				Print("[PQD] PQD_IsPlayerLoadoutValid: StorageComponent not found on server", LogLevel.DEBUG);
				return false;
			}
			
			// Get the local player's info
			PlayerController pc = GetGame().GetPlayerController();
			if (!pc)
			{
				Print("[PQD] PQD_IsPlayerLoadoutValid: PlayerController not found", LogLevel.DEBUG);
				return false;
			}
			
			int playerId = pc.GetPlayerId();
			
			// Try to get the loadout data - if successful, the loadout is valid
			string prefab, loadoutData, requiredRank;
			float cost;
			
			return storageComp.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank);
		}
		else
		{
			// On client: use the client-side cache
			// The cache is populated by the server when player connects or saves loadouts
			return PQD_ClientLoadoutCache.IsSlotValid(factionKey, slotIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Convert slot ID string to index (Slot1 -> 0, Slot2 -> 1, etc.)
	protected int PQD_GetSlotIndex(string slotId)
	{
		if (slotId == "Slot1") return 0;
		if (slotId == "Slot2") return 1;
		if (slotId == "Slot3") return 2;
		if (slotId == "Slot4") return 3;
		if (slotId == "Slot5") return 4;
		return -1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Apply serialized loadout data to a character entity
	protected bool PQD_ApplyLoadoutToEntity(GameEntity playerEntity, string loadoutData, string expectedFactionKey)
	{
		if (loadoutData.IsEmpty())
		{
			Print("[PQD] PQD_ApplyLoadoutToEntity: Loadout data is empty", LogLevel.WARNING);
			return false;
		}
		
		// Create JSON load context
		SCR_JsonLoadContext context = new SCR_JsonLoadContext();
		
		if (!context.ImportFromString(loadoutData))
		{
			Print("[PQD] PQD_ApplyLoadoutToEntity: Failed to parse loadout data", LogLevel.ERROR);
			return false;
		}
		
		// Verify faction key matches
		string factionKey;
		if (context.ReadValue(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTION_KEY, factionKey))
		{
			if (!expectedFactionKey.IsEmpty() && factionKey != expectedFactionKey)
			{
				Print(string.Format("[PQD] PQD_ApplyLoadoutToEntity: Faction mismatch - saved: %1, expected: %2", 
					factionKey, expectedFactionKey), LogLevel.WARNING);
				return false;
			}
		}
		
		// CRITICAL: Ensure ARSENALLOADOUT_COMPONENTS_TO_CHECK is initialized
		if (!SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK || SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK.IsEmpty())
		{
			Print("[PQD] PQD_ApplyLoadoutToEntity: Initializing ARSENALLOADOUT_COMPONENTS_TO_CHECK", LogLevel.WARNING);
			SCR_ArsenalManagerComponent.GetArsenalLoadoutComponentsToCheck(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_COMPONENTS_TO_CHECK);
		}
		
		// Apply the loadout to the entity using the proper static method
		// This handles all the inventory items, storages, and character data properly
		if (!SCR_PlayerArsenalLoadout.ApplyLoadoutString(playerEntity, context))
		{
			Print("[PQD] PQD_ApplyLoadoutToEntity: Failed to apply loadout to entity", LogLevel.ERROR);
			return false;
		}
		
		Print("[PQD] PQD_ApplyLoadoutToEntity: Loadout applied successfully", LogLevel.DEBUG);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the PQD slot ID
	string GetPQDSlotId()
	{
		return m_sPQDSlotId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if this is a PQD loadout slot
	bool IsPQDLoadoutSlot()
	{
		return !m_sPQDSlotId.IsEmpty();
	}
}
