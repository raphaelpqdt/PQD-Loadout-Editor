// PQD Loadout Editor - Modded Arsenal Manager Component
// Author: PQD Team
// Version: 1.0.0
// Adds methods for loadout cost calculation

modded class SCR_ArsenalManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Get the respawn cost of a serialized loadout
	//! \param loadout The serialized loadout string
	//! \param faction The faction to calculate cost for
	//! \param[out] cost The calculated cost
	//! \return True if cost was calculated successfully
	bool PQD_GetLoadoutRespawnCost(string loadout, SCR_Faction faction, out float cost)
	{
		if (!faction)
			return false;
		
		SCR_ArsenalPlayerLoadout playerLoadout = new SCR_ArsenalPlayerLoadout;
		playerLoadout.loadout = loadout;
		playerLoadout.suppliesCost = 0.0;
		
		SCR_JsonLoadContext context = new SCR_JsonLoadContext(false);
		if (!context.ImportFromString(playerLoadout.loadout))
			return false;
		
		SCR_PlayerArsenalLoadout.ComputeSuppliesCost(context, faction, playerLoadout, SCR_EArsenalSupplyCostType.RESPAWN_COST);

		cost = playerLoadout.suppliesCost;
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if player can save a loadout (wrapper for base class method)
	bool PQD_CanSaveLoadout(int playerId, GameEntity characterEntity, FactionAffiliationComponent playerFactionAffiliation, SCR_ArsenalComponent arsenalComponent, bool sendNotificationOnFailed)
	{
		return CanSaveLoadout(playerId, characterEntity, playerFactionAffiliation, arsenalComponent, sendNotificationOnFailed);
	}
}
