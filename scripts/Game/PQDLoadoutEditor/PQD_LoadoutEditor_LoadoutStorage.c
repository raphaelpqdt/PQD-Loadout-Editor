// PQD Loadout Editor - Loadout Storage System
// Author: PQD Team
// Version: 1.1.0

//------------------------------------------------------------------------------------------------
//! Helper class for getting current Unix timestamp
sealed class PQD_TimeHelper
{
	//------------------------------------------------------------------------------------------------
	//! Get current Unix timestamp (seconds since epoch)
	static int GetCurrentTimestamp()
	{
		int year, month, day, hour, minute, second;
		System.GetYearMonthDay(year, month, day);
		System.GetHourMinuteSecond(hour, minute, second);
		
		// Simplified timestamp calculation (not 100% accurate but good enough for ordering)
		// Days since epoch approximation
		int days = (year - 1970) * 365 + (month - 1) * 30 + day;
		return days * 86400 + hour * 3600 + minute * 60 + second;
	}
}

//------------------------------------------------------------------------------------------------
// Storage for a player's loadouts across factions
sealed class PQD_PlayerFactionLoadoutStorage
{
	static int MAX_LOADOUTS_PER_PLAYER = 5;
	static int MAX_ADMIN_LOADOUTS = 10;
	static int CURRENT_FORMAT_VERSION = 1;
	
	// key: factionKey -> slotId -> loadout
	ref map<string, ref map<int, ref PQD_PlayerLoadout>> playerLoadouts = new map<string, ref map<int, ref PQD_PlayerLoadout>>;
	
	// Format version for migration compatibility
	int storageFormatVersion = CURRENT_FORMAT_VERSION;
	
	//------------------------------------------------------------------------------------------------
	static bool SerializeCharacter(IEntity characterEntity, out string serialized)
	{
		GameEntity ent = GameEntity.Cast(characterEntity);
		if (!ent)
		{
			Print(string.Format("[PQD] Character entity is not a GameEntity: %1", characterEntity), LogLevel.ERROR);
			return false;
		}
		
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		if (!SCR_PlayerArsenalLoadout.ReadLoadoutString(characterEntity, saveContext))
		{
			Print(string.Format("[PQD] Failed to serialize GameEntity: %1", ent), LogLevel.ERROR);
			return false;
		}
		
		serialized = saveContext.ExportToString();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static void FillPlayerLoadoutWeaponMetadata(BaseInventoryStorageComponent storage, out string outWeapons)
	{
		int slotsCount = storage.GetSlotsCount();
		
		InventoryStorageSlot slot;
		BaseWeaponComponent weapon;
		string weaponSlotName;
		string extractedWeaponName;
	
		array<string> words = {};
		
		for (int i = 0; i < slotsCount; i++)
		{
			slot = storage.GetSlot(i);
			
			if (!slot.GetAttachedEntity())
				continue;
			
			weapon = BaseWeaponComponent.Cast(slot.GetParentContainer());
			if (!weapon)
				continue;
			
			if (!PQD_Helpers.GetWeaponSlotTypeString(weapon, weaponSlotName))
				continue;
			
			if (weaponSlotName == "primary" || weaponSlotName == "secondary")
			{
				if (!PQD_Helpers.GetItemNameFromEntityValidate(slot.GetAttachedEntity(), extractedWeaponName))
					continue;
				
				words.Insert(extractedWeaponName);
			}
		}
		
		outWeapons = SCR_StringHelper.Join("\n", words, false);
	}
	
	//------------------------------------------------------------------------------------------------
	static void FillPlayerLoadoutClothesMetadata(BaseInventoryStorageComponent storage, out string outClothes)
	{
		int slotsCount = storage.GetSlotsCount();
		
		InventoryStorageSlot slot;
		typename areaType;
		string extractedName;
		
		array<string> words = {};
	
		for (int i = 0; i < slotsCount; i++)
		{
			slot = storage.GetSlot(i);
			
			if (!slot.GetAttachedEntity())
				continue;
			
			if (!PQD_Helpers.GetLoadoutAreaType(slot, areaType))
				continue;
					
			if (areaType == LoadoutHeadCoverArea || areaType == LoadoutJacketArea || 
				areaType == LoadoutVestArea || areaType == LoadoutPantsArea)
			{
				if (!PQD_Helpers.GetItemNameFromEntityValidate(slot.GetAttachedEntity(), extractedName))
					continue;

				words.Insert(extractedName);
			}
		}
		
		outClothes = SCR_StringHelper.Join("\n", words, false);
	}
	
	//------------------------------------------------------------------------------------------------
	static bool FillPlayerLoadoutMetadata(IEntity ent, out string outClothes, out string outWeapons, out string outRank)
	{
		array<BaseInventoryStorageComponent> storages = {};
		
		int storagesCount = PQD_Helpers.GetAllEntityStorages(ent, storages);
		if (storagesCount < 1)
			return false;
		
		outWeapons = "";
		outClothes = "";

		array<IEntity> itemsInLoadout = {};

		PQD_StorageType storageType;
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			storageType = PQD_Helpers.GetStorageType(storage);
			storage.GetAll(itemsInLoadout);
			
			switch (storageType)
			{
				case PQD_StorageType.CHARACTER_WEAPON:
					FillPlayerLoadoutWeaponMetadata(storage, outWeapons);
					break;
				case PQD_StorageType.CHARACTER_LOADOUT:
					FillPlayerLoadoutClothesMetadata(storage, outClothes);
					break;
			}
		}
		
		if (outWeapons.IsEmpty())
			outWeapons = "N/A";
		
		if (outClothes.IsEmpty())
			outClothes = "N/A";
		
		SCR_ECharacterRank loadoutRequiredRank = SCR_ECharacterRank.RENEGADE;
		if (itemsInLoadout.Count() > 0)
		{
			ResourceName prefab;
			SCR_ECharacterRank rank;

			foreach (IEntity item : itemsInLoadout)
			{
				if (!PQD_Helpers.GetResourceNameFromEntity(item, prefab))
					continue;
				
				rank = PQD_Helpers.GetItemRequiredRankFromCache(prefab);
				
				if (rank == SCR_ECharacterRank.INVALID)
					continue;

				if (rank > loadoutRequiredRank)
					loadoutRequiredRank = rank;
			}
		}

		outRank = typename.EnumToString(SCR_ECharacterRank, loadoutRequiredRank);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool SaveLoadout(SCR_ArsenalManagerComponent arsenalManager, IEntity characterEntity, string factionKey, int slotId)
	{
		PQD_PlayerLoadout newPlayerLoadout = new PQD_PlayerLoadout();
		
		if (!PQD_Helpers.GetResourceNameFromEntity(characterEntity, newPlayerLoadout.prefab))
		{
			Print(string.Format("[PQD] Failed to get resource name from entity: %1", characterEntity), LogLevel.ERROR);
			return false;
		}

		Print(string.Format("[PQD] SaveLoadout: Got prefab from character: '%1'", newPlayerLoadout.prefab), LogLevel.NORMAL);

		newPlayerLoadout.slotId = slotId;
		newPlayerLoadout.formatVersion = CURRENT_FORMAT_VERSION;
		
		// Set timestamps
		int currentTime = PQD_TimeHelper.GetCurrentTimestamp();
		newPlayerLoadout.modifiedAt = currentTime;
		
		// Check if updating existing loadout (preserve createdAt) or creating new
		PQD_PlayerLoadout existingLoadout;
		if (GetLoadout(factionKey, slotId, existingLoadout) && existingLoadout.HasData())
			newPlayerLoadout.createdAt = existingLoadout.createdAt;
		else
			newPlayerLoadout.createdAt = currentTime;
		
		if (!PQD_PlayerFactionLoadoutStorage.FillPlayerLoadoutMetadata(characterEntity, newPlayerLoadout.metadata_clothes, newPlayerLoadout.metadata_weapons, newPlayerLoadout.required_rank))
		{
			Print("[PQD] Failed to fill loadout metadata", LogLevel.ERROR);
			return false;
		}
		
		if (!PQD_PlayerFactionLoadoutStorage.SerializeCharacter(characterEntity, newPlayerLoadout.data))
		{
			Print("[PQD] Failed to serialize character", LogLevel.ERROR);
			return false;
		}
		
		// Validate serialized data
		if (newPlayerLoadout.data.IsEmpty())
		{
			Print("[PQD] Serialized data is empty", LogLevel.ERROR);
			return false;
		}
		
		// Calculate supply cost
		if (factionKey != "admin")
		{
			SCR_Faction faction = PQD_Helpers.GetFactionFromFactionKey(factionKey);
			if (!faction)
			{
				Print(string.Format("[PQD] Invalid faction '%1'", factionKey), LogLevel.ERROR);
				return false;
			}
			
			float cost;
			if (!arsenalManager.PQD_GetLoadoutRespawnCost(newPlayerLoadout.data, faction, cost))
			{
				Print("[PQD] Could not compute loadout cost", LogLevel.WARNING);
				// Continue anyway - cost calculation is not critical
				cost = 0;
			}
			newPlayerLoadout.supplyCost = cost;
		}

		if (!playerLoadouts.Contains(factionKey))
			playerLoadouts.Set(factionKey, new map<int, ref PQD_PlayerLoadout>());
		
		auto playerFactionLoadouts = playerLoadouts.Get(factionKey);
		playerFactionLoadouts.Set(slotId, newPlayerLoadout);
		
		Print(string.Format("[PQD] Loadout saved successfully - slot %1, faction %2, weapons: %3", 
			slotId, factionKey, newPlayerLoadout.metadata_weapons), LogLevel.DEBUG);
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetLoadout(string factionKey, int slotId, out PQD_PlayerLoadout playerLoadout)
	{
		if (!playerLoadouts.Contains(factionKey))
			return false;
		
		auto playerFactionLoadouts = playerLoadouts.Get(factionKey);
		if (!playerFactionLoadouts.Contains(slotId))
			return false;
		
		playerLoadout = playerFactionLoadouts.Get(slotId);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ClearLoadoutSlot(string factionKey, int slotId)
	{
		if (!playerLoadouts.Contains(factionKey))
			return false;
		
		auto playerFactionLoadouts = playerLoadouts.Get(factionKey);
		if (!playerFactionLoadouts.Contains(slotId))
			return false;
		
		PQD_PlayerLoadout option = new PQD_PlayerLoadout();
		option.slotId = slotId;
		option.Clear(); // Use the new Clear method
		
		playerFactionLoadouts.Set(slotId, option);
		
		Print(string.Format("[PQD] Loadout slot %1 cleared for faction %2", slotId, factionKey), LogLevel.DEBUG);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void InitLoadouts(string factionKey)
	{
		map<int, ref PQD_PlayerLoadout> loadoutData = new map<int, ref PQD_PlayerLoadout>();
		
		PQD_PlayerLoadout option;
		int max;
		if (factionKey == "admin")
			max = PQD_PlayerFactionLoadoutStorage.MAX_ADMIN_LOADOUTS;
		else
			max = PQD_PlayerFactionLoadoutStorage.MAX_LOADOUTS_PER_PLAYER;
		
		for (int x = 0; x < max; x++)
		{
			option = new PQD_PlayerLoadout();
			option.slotId = x;
			loadoutData.Set(x, option);
		}
		
		playerLoadouts.Set(factionKey, loadoutData);
	}
	
	//------------------------------------------------------------------------------------------------
	void GetPlayerLoadoutOptions(string factionKey, out array<ref PQD_PlayerLoadout> loadoutData)
	{
		if (!playerLoadouts.Contains(factionKey))
			InitLoadouts(factionKey);
		
		auto playerFactionLoadouts = playerLoadouts.Get(factionKey);
		
		int max;
		if (factionKey == "admin")
			max = PQD_PlayerFactionLoadoutStorage.MAX_ADMIN_LOADOUTS;
		else
			max = PQD_PlayerFactionLoadoutStorage.MAX_LOADOUTS_PER_PLAYER;
		
		for (int x = 0; x < max; x++)
		{
			PQD_PlayerLoadout loadout = playerFactionLoadouts.Get(x);

			// Clone the loadout to avoid reference issues
			PQD_PlayerLoadout option = new PQD_PlayerLoadout();
			option.slotId = loadout.slotId;
			option.metadata_clothes = loadout.metadata_clothes;
			option.metadata_weapons = loadout.metadata_weapons;
			option.loadoutName = loadout.loadoutName;
			option.supplyCost = loadout.supplyCost;
			option.required_rank = loadout.required_rank;
			option.createdAt = loadout.createdAt;
			option.modifiedAt = loadout.modifiedAt;
			option.formatVersion = loadout.formatVersion;
			// Copy prefab and data for 3D preview in loadout editor
			option.prefab = loadout.prefab;
			option.data = loadout.data;

			loadoutData.Insert(option);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Validate that the storage data is not corrupted
	bool ValidateStorageIntegrity()
	{
		foreach (string factionKey, map<int, ref PQD_PlayerLoadout> factionLoadouts : playerLoadouts)
		{
			foreach (int slotId, PQD_PlayerLoadout loadout : factionLoadouts)
			{
				if (loadout.slotId != slotId)
				{
					Print(string.Format("[PQD] Data integrity issue: slot ID mismatch (expected %1, got %2)", slotId, loadout.slotId), LogLevel.WARNING);
					loadout.slotId = slotId; // Fix the issue
				}
			}
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
sealed class PQD_LoadoutStorageComponentClass : SCR_BaseGameModeComponentClass {}

//------------------------------------------------------------------------------------------------
sealed class PQD_LoadoutStorageComponent : SCR_BaseGameModeComponent
{
	// Storage by player id
	private ref map<int, ref PQD_PlayerFactionLoadoutStorage> loadoutStorage = new map<int, ref PQD_PlayerFactionLoadoutStorage>();
	
	// Base path for loadout files - using versioned folder for future compatibility
	protected string loadoutPathRoot = "$profile:/PQDLoadoutEditor_Loadouts/1.1.0";
	
	// Backup path for old version migration
	protected string loadoutPathLegacy = "$profile:/PQDLoadoutEditor_Loadouts/1.0.0";

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (loadoutStorage.Contains(playerId))
		{
			Print(string.Format("[PQD] Player %1 disconnected, removing loadout cache", playerId), LogLevel.DEBUG);
			loadoutStorage.Remove(playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Ensure the storage directories exist
	protected void EnsureDirectoryExists(string path)
	{
		if (!FileIO.FileExists(path))
			FileIO.MakeDirectory(path);
	}
	
	//------------------------------------------------------------------------------------------------
	bool SaveCurrentPlayerLoadout(SCR_ArsenalManagerComponent arsenalManager, int playerId, string identityId, IEntity characterEntity, string factionKey, int slotId, bool isAdminLoadout = false)
	{
		// Validate inputs
		if (!characterEntity)
		{
			Print("[PQD] SaveCurrentPlayerLoadout: Character entity is null", LogLevel.ERROR);
			return false;
		}
		
		if (identityId.IsEmpty() && !isAdminLoadout)
		{
			Print("[PQD] SaveCurrentPlayerLoadout: Identity ID is empty", LogLevel.ERROR);
			return false;
		}
		
		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
		}
		
		if (!loadoutStorage.Contains(playerId))
			loadoutStorage.Set(playerId, new PQD_PlayerFactionLoadoutStorage());
		
		auto playerLoadoutStorage = loadoutStorage.Get(playerId);
		
		if (!playerLoadoutStorage.SaveLoadout(arsenalManager, characterEntity, factionKey, slotId))
		{
			Print(string.Format("[PQD] Failed to save loadout for player %1, slot %2", playerId, slotId), LogLevel.ERROR);
			return false;
		}
		
		Print(string.Format("[PQD] Saving loadouts to file for faction %1, identity %2", factionKey, identityId), LogLevel.DEBUG);
		
		bool fileSaved = SavePlayerLoadoutToFile(playerId, identityId, factionKey, isAdminLoadout);
		if (!fileSaved)
		{
			Print("[PQD] Warning: Loadout saved to memory but file save failed", LogLevel.WARNING);
			// Still return true since memory save worked - file save failure is not critical during session
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ClearLoadoutSlot(int playerId, string identityId, string factionKey, int slotId, bool isAdminLoadout = false)
	{
		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
		}

		if (!loadoutStorage.Contains(playerId))
		{
			Print(string.Format("[PQD] ClearLoadoutSlot: No storage found for player %1", playerId), LogLevel.WARNING);
			return false;
		}

		auto playerLoadoutStorage = loadoutStorage.Get(playerId);
		
		if (!playerLoadoutStorage.ClearLoadoutSlot(factionKey, slotId))
		{
			Print(string.Format("[PQD] ClearLoadoutSlot: Failed to clear slot %1 for player %2", slotId, playerId), LogLevel.ERROR);
			return false;
		}
		
		return SavePlayerLoadoutToFile(playerId, identityId, factionKey, isAdminLoadout);
	}

	//------------------------------------------------------------------------------------------------
	//! Save player loadouts to JSON file
	bool SavePlayerLoadoutToFile(int playerId, string identityId, string factionKey, bool isAdminLoadout = false)
	{
		if (!PQD_Helpers.IsFileSavingEnabled())
		{
			Print("[PQD] File saving is disabled on this platform", LogLevel.DEBUG);
			return true;
		}
		
		// Build the directory path
		string path = loadoutPathRoot;
		EnsureDirectoryExists(path);
		
		string fileName;
		
		if (isAdminLoadout)
		{
			playerId = -100;
			fileName = "admin_loadouts";
		}
		else
		{
			// Create faction subdirectory
			path = string.Format("%1/%2", path, factionKey);
			EnsureDirectoryExists(path);
			
			// Create identity prefix subdirectory (for better file organization)
			if (identityId.Length() >= 2)
			{
				path = string.Format("%1/%2", path, identityId.Substring(0, 2));
				EnsureDirectoryExists(path);
			}
			
			fileName = identityId;
		}
		
		// Get the storage to save
		if (!loadoutStorage.Contains(playerId))
		{
			Print(string.Format("[PQD] SavePlayerLoadoutToFile: No storage found for player %1", playerId), LogLevel.ERROR);
			return false;
		}
		
		// Serialize to JSON
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		
		if (!ctx.WriteValue("", loadoutStorage.Get(playerId)))
		{
			Print("[PQD] SavePlayerLoadoutToFile: Failed to serialize loadout data", LogLevel.ERROR);
			return false;
		}
		
		string fullPath = string.Format("%1/%2", path, fileName);
		
		if (!ctx.SaveToFile(fullPath))
		{
			Print(string.Format("[PQD] SavePlayerLoadoutToFile: Failed to write to file: %1", fullPath), LogLevel.ERROR);
			return false;
		}
		
		Print(string.Format("[PQD] Loadouts saved to: %1", fullPath), LogLevel.DEBUG);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Load player loadouts from JSON file
	bool LoadPlayerLoadoutFromFile(int playerId, string identityId, string factionKey, bool isAdminLoadout = false)
	{
		if (!PQD_Helpers.IsFileSavingEnabled())
		{
			Print("[PQD] File loading is disabled on this platform", LogLevel.DEBUG);
			return false;
		}

		string path;
		string fileName;

		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
			fileName = "admin_loadouts";
			path = string.Format("%1/%2", loadoutPathRoot, fileName);
		}
		else
		{
			if (identityId.Length() < 2)
			{
				Print(string.Format("[PQD] LoadPlayerLoadoutFromFile: Invalid identity ID: %1", identityId), LogLevel.ERROR);
				return false;
			}
			path = string.Format("%1/%2/%3/%4", loadoutPathRoot, factionKey, identityId.Substring(0, 2), identityId);
		}

		// Try current version path first
		if (!FileIO.FileExists(path))
		{
			// Try legacy path for migration
			string legacyPath;
			if (isAdminLoadout)
				legacyPath = string.Format("%1/admin_loadouts", loadoutPathLegacy);
			else
				legacyPath = string.Format("%1/%2/%3/%4", loadoutPathLegacy, factionKey, identityId.Substring(0, 2), identityId);
			
			if (FileIO.FileExists(legacyPath))
			{
				Print(string.Format("[PQD] Found legacy loadout file, migrating: %1", legacyPath), LogLevel.NORMAL);
				path = legacyPath;
				// After loading, we'll save to new path to migrate
			}
			else
			{
				Print(string.Format("[PQD] No loadout file found at: %1", path), LogLevel.DEBUG);
				return false;
			}
		}
		
		// Load from file
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		
		if (!ctx.LoadFromFile(path))
		{
			Print(string.Format("[PQD] LoadPlayerLoadoutFromFile: Failed to load file: %1", path), LogLevel.ERROR);
			return false;
		}
		
		PQD_PlayerFactionLoadoutStorage playerLoadoutStorage = new PQD_PlayerFactionLoadoutStorage();
		
		if (!ctx.ReadValue("", playerLoadoutStorage))
		{
			Print(string.Format("[PQD] LoadPlayerLoadoutFromFile: Failed to deserialize data from: %1", path), LogLevel.ERROR);
			return false;
		}
		
		// Validate loaded data
		playerLoadoutStorage.ValidateStorageIntegrity();
		
		Print(string.Format("[PQD] Loaded loadout from file for faction %1, identity %2", factionKey, identityId), LogLevel.DEBUG);
		
		loadoutStorage.Set(playerId, playerLoadoutStorage);
		
		// If we loaded from legacy path, save to new path for migration
		if (path.Contains(loadoutPathLegacy))
		{
			Print("[PQD] Migrating loadout to new storage path", LogLevel.NORMAL);
			SavePlayerLoadoutToFile(playerId, identityId, factionKey, isAdminLoadout);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetPlayerLoadoutData(int playerId, string factionKey, int slotId, out string prefab, out string loadoutData, out float cost, bool isAdminLoadout = false, out string requiredRank = "")
	{
		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
		}
		
		if (!loadoutStorage.Contains(playerId))
		{
			// Try to load from file first
			string identityId = "";
			if (!isAdminLoadout)
			{
				identityId = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
				if (identityId.IsEmpty())
				{
					Print(string.Format("[PQD] GetPlayerLoadoutData: Could not get identity ID for player %1", playerId), LogLevel.WARNING);
					return false;
				}
			}
			
			if (!LoadPlayerLoadoutFromFile(playerId, identityId, factionKey, isAdminLoadout))
			{
				Print(string.Format("[PQD] GetPlayerLoadoutData: No storage found for player %1 and no file to load", playerId), LogLevel.WARNING);
				return false;
			}
		}
		
		auto playerLoadoutStorage = loadoutStorage.Get(playerId);
		
		PQD_PlayerLoadout playerLoadout;
		if (!playerLoadoutStorage.GetLoadout(factionKey, slotId, playerLoadout))
		{
			Print(string.Format("[PQD] GetPlayerLoadoutData: No loadout found at slot %1 for player %2", slotId, playerId), LogLevel.WARNING);
			return false;
		}
		
		// Validate the loadout has actual data
		Print(string.Format("[PQD] GetPlayerLoadoutData: Slot %1, HasData=%2, data='%3', prefab='%4'",
			slotId, playerLoadout.HasData(), playerLoadout.data.Length(), playerLoadout.prefab), LogLevel.NORMAL);

		if (!playerLoadout.HasData())
		{
			Print(string.Format("[PQD] GetPlayerLoadoutData: Loadout at slot %1 has no data", slotId), LogLevel.WARNING);
			return false;
		}

		loadoutData = playerLoadout.data;
		prefab = playerLoadout.prefab;
		cost = playerLoadout.supplyCost;
		requiredRank = playerLoadout.required_rank;
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void GetPlayerLoadoutMetadata(int playerId, string identityId, string factionKey, out array<ref PQD_PlayerLoadout> loadoutData, bool isAdminLoadout = false)
	{
		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
		}
		
		if (!loadoutStorage.Contains(playerId))
		{
			// Try to load from file first
			if (!LoadPlayerLoadoutFromFile(playerId, identityId, factionKey, isAdminLoadout))
			{
				// Create new empty storage if no file exists
				Print(string.Format("[PQD] Creating new loadout storage for player %1, faction %2", playerId, factionKey), LogLevel.DEBUG);
				loadoutStorage.Set(playerId, new PQD_PlayerFactionLoadoutStorage());
			}
		}
		
		auto playerLoadoutStorage = loadoutStorage.Get(playerId);
		playerLoadoutStorage.GetPlayerLoadoutOptions(factionKey, loadoutData);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get all players with cached loadout data (for debugging/admin purposes)
	int GetCachedPlayerCount()
	{
		return loadoutStorage.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Force reload loadouts from file for a specific player (useful for debugging)
	bool ForceReloadPlayerLoadouts(int playerId, string identityId, string factionKey, bool isAdminLoadout = false)
	{
		if (isAdminLoadout)
		{
			playerId = -100;
			factionKey = "admin";
		}
		
		// Remove from cache to force reload
		if (loadoutStorage.Contains(playerId))
			loadoutStorage.Remove(playerId);
		
		return LoadPlayerLoadoutFromFile(playerId, identityId, factionKey, isAdminLoadout);
	}
}
