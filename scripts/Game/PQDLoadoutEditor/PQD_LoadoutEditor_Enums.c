// PQD Loadout Editor - Enumerations and Data Structures
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
// Menu preset enum extension
modded enum ChimeraMenuPreset
{
	PQD_LoadoutEditor,
};

//------------------------------------------------------------------------------------------------
// Editor operation modes
sealed enum PQD_EditorMode
{
	CHARACTER,		// Viewing full character
	ENTITY,			// Viewing specific entity (weapon, equipment)
	STORAGE,		// Viewing storage contents
	OPTIONS,		// Editor settings
	LOADOUTS,		// Player saved loadouts
	SERVER_LOADOUTS	// Admin/Server loadouts
}

//------------------------------------------------------------------------------------------------
// Current input/view state
sealed enum PQD_InputMode
{
	VIEW_SLOTS,
	SWAP_SLOT,
	VIEW_OPTIONS,
	VIEW_ITEM_STORAGE_CONTENT,
	VIEW_ITEM_STORAGES,
	VIEW_ITEM_ARSENAL
}

//------------------------------------------------------------------------------------------------
// Storage type classification
sealed enum PQD_StorageType
{
	DEFAULT,
	CHARACTER_LOADOUT,
	CHARACTER_EQUIPMENT,
	CHARACTER_WEAPON,
	CHARACTER_IDENTITY,
	ATTACHMENT_STORAGE,
	WEAPON,
	LOADOUTSLOTINFO,
	ATTACHMENTSLOT,
	INVENTORY,
	OPTIONS,
	IGNORED
}

//------------------------------------------------------------------------------------------------
// Slot type classification for UI and logic
sealed enum PQD_SlotType
{
	CHARACTER_LOADOUT,
	CHARACTER_WEAPON,
	CHARACTER_EQUIPMENT,
	CHARACTER_VISUAL_IDENTITY,
	CHARACTER_SOUND_IDENTITY,
	INVENTORY,
	ATTACHMENT,
	MAGAZINE,
	REMOVE,
	ARSENAL_ITEM,
	ARSENAL_ITEM_ARSENAL,
	ARSENAL_ITEM_BACK,
	ADD,
	OPTION,
	MESSAGE,
	LOADOUT_SAVE,
	LOADOUT_LOAD,
	UNKNOWN
}

//------------------------------------------------------------------------------------------------
// Category tabs for the editor
sealed enum PQD_SlotCategory
{
	CHARACTER_CLOTHING,
	CHARACTER_GEAR,
	CHARACTER_ITEMS,
	SETTINGS,
	LOADOUTS,
	SERVER_LOADOUTS
}

//------------------------------------------------------------------------------------------------
// Network action types
enum PQD_ActionType
{
	ADD_ITEM,
	REMOVE_ITEM,
	REPLACE_ITEM,
	GET_LOADOUTS,
	SAVE_LOADOUT,
	CLEAR_LOADOUT,
	APPLY_LOADOUT,
	GET_ADMIN_LOADOUTS,
	SAVE_LOADOUT_ADMIN,
	APPLY_LOADOUT_ADMIN,
	SET_AI_LOADOUT_ADMIN,
	CLEAR_LOADOUT_ADMIN,
	CHANGE_VISUAL_IDENTITY,
	CHANGE_SOUND_IDENTITY
}

//------------------------------------------------------------------------------------------------
// Camera pan modes
enum PQD_PanMode
{
	NONE,
	CAMERA,
	LIGHTS
}

//------------------------------------------------------------------------------------------------
// Status message types
enum PQD_MessageType
{
	OK,
	WARNING,
	ERROR
}

//------------------------------------------------------------------------------------------------
// Processing step for error handling
enum PQD_ProcessingStep
{
	DEFAULT,
	MENU_OPEN,
	MENU_INIT,
	CREATE_SLOT
}

//------------------------------------------------------------------------------------------------
// Item categories for the Items tab
enum PQD_ItemCategory
{
	AMMUNITION,
	EQUIPMENT,
	MEDICAL
}
