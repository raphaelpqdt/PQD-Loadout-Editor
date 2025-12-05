// PQD Loadout Editor - Network Classes
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
// Base network request
class PQD_NetworkRequest
{
	PQD_ActionType actionType;
	
	string Repr()
	{
		return string.Format("action: %1", SCR_Enum.GetEnumName(PQD_ActionType, actionType));
	}
}

//------------------------------------------------------------------------------------------------
// Storage operation request
sealed class PQD_StorageRequest : PQD_NetworkRequest
{
	RplId arsenalEntityRplId;
	RplId storageRplId;
	int storageSlotId = -1;
	ResourceName prefab = "";
	
	override string Repr()
	{
		return string.Format("action: %1, rplId: %2, slotId: %3, prefab: %4", 
			SCR_Enum.GetEnumName(PQD_ActionType, actionType), storageRplId, storageSlotId, prefab);
	}
}

//------------------------------------------------------------------------------------------------
// Loadout operation request
sealed class PQD_LoadoutRequest : PQD_NetworkRequest
{
	int loadoutSlotId = -1;
	RplId arsenalComponentRplId;
}

//------------------------------------------------------------------------------------------------
// Network response
sealed class PQD_NetworkResponse
{
	bool success;
	string message;
	ref PQD_NetworkRequest request;
}
