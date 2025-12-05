// PQD Loadout Editor - Data Classes
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
// Base slot item class
class PQD_SlotItem
{
	PQD_SlotType slotType;
	int listBoxChildId;
}

//------------------------------------------------------------------------------------------------
// Slot choice base class - represents an option in a slot
class PQD_SlotChoice : PQD_SlotItem
{
	ResourceName prefab;
	string slotName = "";
	string itemName = "";
	bool slotEnabled = true;
}

//------------------------------------------------------------------------------------------------
// Slot info class - contains full slot information
class PQD_SlotInfo : PQD_SlotChoice
{
	int storageRplId;
	int storageSlotId;
	PQD_StorageType storageType;
	InventoryStorageSlot slot;
	bool hasStorage = false;
}

//------------------------------------------------------------------------------------------------
// Identity slot for character visual/sound identity
sealed class PQD_SlotIdentity : PQD_SlotInfo
{
	string factionKey;
}

//------------------------------------------------------------------------------------------------
// Storage item listing for inventory view
sealed class PQD_SlotInfoStorageItem : PQD_SlotInfo
{
	int numItems = 1;
}

//------------------------------------------------------------------------------------------------
// Loadout slot for saved loadouts (UI display class)
sealed class PQD_SlotLoadout : PQD_SlotItem
{
	string metadataClothes = "";
	string metadataWeapons = "";
	string loadoutName = "";           // Custom name if set
	ResourceName prefab;               // Character prefab for 3D preview
	string data = "";                  // Serialized loadout data for equipped preview
	float supplyCost;
	int loadoutSlotId = -1;
	SCR_ECharacterRank requiredRank = SCR_ECharacterRank.INVALID;

	// Timestamps for display
	int createdAt;
	int modifiedAt;
	
	//------------------------------------------------------------------------------------------------
	//! Check if this loadout slot has actual data
	bool HasData()
	{
		return !metadataWeapons.IsEmpty() && metadataWeapons != "N/A";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get a formatted display name for UI
	string GetDisplayName()
	{
		if (!loadoutName.IsEmpty())
			return loadoutName;
		
		if (HasData())
		{
			// Use first weapon name
			array<string> weapons = {};
			metadataWeapons.Split("\n", weapons, false);
			if (weapons.Count() > 0)
				return string.Format("Slot %1: %2", loadoutSlotId + 1, weapons[0]);
		}
		
		return string.Format("Slot %1 (Empty)", loadoutSlotId + 1);
	}
}

//------------------------------------------------------------------------------------------------
// Identity choice for visual/sound identity selection
sealed class PQD_SlotChoiceIdentity : PQD_SlotChoice
{
	ResourceName prefabBody;
	string factionKey;
	int factionIdentityId;
}

//------------------------------------------------------------------------------------------------
// Item category choice for navigable categories in Items tab
sealed class PQD_SlotChoiceCategory : PQD_SlotChoice
{
	PQD_ItemCategory category;
}

//------------------------------------------------------------------------------------------------
// Editor option data
sealed class PQD_EditorOptionData : PQD_SlotChoice
{
	string editorOptionLabel;
	string optionLabel;
	string optionValue;
	ResourceName optionPrefab;
	bool optionEnabled = true;
}

//------------------------------------------------------------------------------------------------
// Item listing helper class
sealed class PQD_ItemListing
{
	int numItems;
	PQD_SlotInfo slotInfo;
}

//------------------------------------------------------------------------------------------------
// Arsenal item details cache
sealed class PQD_ArsenalItemDetails
{
	float supplyCost;
	SCR_ECharacterRank requiredRank;
}

//------------------------------------------------------------------------------------------------
// Player loadout data structure
sealed class PQD_PlayerLoadout
{
	// Metadata for display
	string metadata_clothes;
	string metadata_weapons;
	string loadoutName;          // Custom name set by player
	
	// Serialized loadout data
	string prefab;               // Character prefab ResourceName
	string data;                 // Serialized character data (JSON from SCR_PlayerArsenalLoadout)
	
	// Requirements and cost
	string required_rank;        // Highest rank required for items in loadout
	float supplyCost;            // Total supply cost of the loadout
	
	// Slot identification
	int slotId;
	
	// Timestamps (stored as Unix timestamp)
	int createdAt;               // When the loadout was first saved
	int modifiedAt;              // When the loadout was last modified
	
	// Version for future format compatibility
	int formatVersion = 1;
	
	//------------------------------------------------------------------------------------------------
	//! Check if this loadout slot has actual data saved
	bool HasData()
	{
		return !data.IsEmpty() && !prefab.IsEmpty();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get display name for UI - uses custom name or generates from metadata
	string GetDisplayName()
	{
		if (!loadoutName.IsEmpty())
			return loadoutName;
		
		// Generate from metadata if no custom name
		if (!metadata_weapons.IsEmpty() && metadata_weapons != "N/A")
		{
			// Use first weapon as name
			array<string> weapons = {};
			metadata_weapons.Split("\n", weapons, false);
			if (weapons.Count() > 0)
				return string.Format("Loadout %1: %2", slotId + 1, weapons[0]);
		}
		
		return string.Format("Loadout Slot %1", slotId + 1);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create a copy of this loadout
	PQD_PlayerLoadout CopyLoadout()
	{
		PQD_PlayerLoadout copy = new PQD_PlayerLoadout();
		copy.metadata_clothes = metadata_clothes;
		copy.metadata_weapons = metadata_weapons;
		copy.loadoutName = loadoutName;
		copy.prefab = prefab;
		copy.data = data;
		copy.required_rank = required_rank;
		copy.supplyCost = supplyCost;
		copy.slotId = slotId;
		copy.createdAt = createdAt;
		copy.modifiedAt = modifiedAt;
		copy.formatVersion = formatVersion;
		return copy;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clear all data in this loadout slot
	void Clear()
	{
		metadata_clothes = "";
		metadata_weapons = "";
		loadoutName = "";
		prefab = "";
		data = "";
		required_rank = "";
		supplyCost = 0;
		createdAt = 0;
		modifiedAt = 0;
	}
}
