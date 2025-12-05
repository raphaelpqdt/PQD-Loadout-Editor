// PQD Loadout Editor - Modded Loadout Manager
// Author: PQD Team
// Version: 1.0.0
// Description: Extends SCR_LoadoutManager to inject PQD loadout slots

//------------------------------------------------------------------------------------------------
//! Modded LoadoutManager class
[EntityEditorProps(category: "GameScripted/GameMode", description: "PQD Loadout Manager extension.", color: "0 0 255 255")]
modded class SCR_LoadoutManagerClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
//! Modded LoadoutManager - injects PQD loadout slots when queried
modded class SCR_LoadoutManager : GenericEntity
{
	// Flag to track if PQD loadouts were added
	protected bool m_bPQDLoadoutsRegistered = false;
	
	//------------------------------------------------------------------------------------------------
	//! Override: Get player loadouts by faction - ensures PQD loadouts are registered first
	override int GetPlayerLoadoutsByFaction(Faction faction, out notnull array<ref SCR_BasePlayerLoadout> outLoadouts)
	{
		// Register PQD loadouts on first use
		PQD_EnsureLoadoutsRegistered();
		
		// Call base implementation
		int result = super.GetPlayerLoadoutsByFaction(faction, outLoadouts);
		
		// Debug: Log what was returned
		string factionKey = "";
		if (faction)
			factionKey = faction.GetFactionKey();
		
		Print(string.Format("[PQD] GetPlayerLoadoutsByFaction: faction=%1, returned %2 loadouts", factionKey, result), LogLevel.NORMAL);
		
		foreach (SCR_BasePlayerLoadout loadout : outLoadouts)
		{
			SCR_PlayerArsenalLoadout arsenalLoadout = SCR_PlayerArsenalLoadout.Cast(loadout);
			if (arsenalLoadout && !arsenalLoadout.m_sPQDSlotId.IsEmpty())
			{
				Print(string.Format("[PQD] - PQD Loadout: %1, faction: %2, slotId: %3", 
					loadout.GetLoadoutName(), arsenalLoadout.m_sAffiliatedFaction, arsenalLoadout.m_sPQDSlotId), LogLevel.NORMAL);
			}
		}
		
		return result;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Ensure PQD loadouts are registered in the array
	protected void PQD_EnsureLoadoutsRegistered()
	{
		if (m_bPQDLoadoutsRegistered)
			return;
		
		m_bPQDLoadoutsRegistered = true;
		
		Print("[PQD] Registering PQD loadout slots...", LogLevel.NORMAL);
		
		// Check if PQD respawn system is enabled
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			Print("[PQD] PQD_EnsureLoadoutsRegistered: GameMode not found", LogLevel.WARNING);
			return;
		}
		
		PQD_RespawnLoadoutComponent respawnComp = PQD_RespawnLoadoutComponent.Cast(gameMode.FindComponent(PQD_RespawnLoadoutComponent));
		if (!respawnComp || !respawnComp.IsEnabled())
		{
			Print("[PQD] PQD_EnsureLoadoutsRegistered: Respawn component not found or disabled", LogLevel.DEBUG);
			return;
		}
		
		// Get the faction manager to get available factions
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			Print("[PQD] PQD_EnsureLoadoutsRegistered: FactionManager not found", LogLevel.WARNING);
			return;
		}
		
		// Get all playable factions
		array<Faction> factions = {};
		factionManager.GetFactionsList(factions);
		
		if (!m_aPlayerLoadouts)
		{
			Print("[PQD] PQD_EnsureLoadoutsRegistered: m_aPlayerLoadouts is null!", LogLevel.ERROR);
			return;
		}
		
		int maxSlots = respawnComp.GetMaxLoadoutsToShow();
		int addedCount = 0;
		
		Print(string.Format("[PQD] Found %1 factions, adding %2 slots per faction", factions.Count(), maxSlots), LogLevel.DEBUG);
		
		// For each faction, add PQD loadout slots
		foreach (Faction faction : factions)
		{
			SCR_Faction scrFaction = SCR_Faction.Cast(faction);
			if (!scrFaction)
				continue;
			
			// Check if faction is playable
			if (!scrFaction.IsPlayable())
			{
				Print(string.Format("[PQD] Skipping non-playable faction: %1", scrFaction.GetFactionKey()), LogLevel.DEBUG);
				continue;
			}
			
			string factionKey = scrFaction.GetFactionKey();
			Print(string.Format("[PQD] Adding slots for faction: %1", factionKey), LogLevel.DEBUG);
			
			// Add loadout slots for this faction
			for (int i = 0; i < maxSlots; i++)
			{
				string slotId = string.Format("Slot%1", i + 1);
				
				// Create a PQD loadout slot
				SCR_PlayerArsenalLoadout pqdLoadout = PQD_CreateLoadoutSlot(scrFaction, slotId, i);
				if (pqdLoadout)
				{
					m_aPlayerLoadouts.Insert(pqdLoadout);
					addedCount++;
					Print(string.Format("[PQD] Added loadout slot: %1 for faction %2", slotId, factionKey), LogLevel.DEBUG);
				}
			}
		}
		
		if (addedCount > 0)
			Print(string.Format("[PQD] Registered %1 PQD loadout slots successfully", addedCount), LogLevel.NORMAL);
		else
			Print("[PQD] No PQD loadout slots were added", LogLevel.WARNING);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create a PQD loadout slot instance
	protected SCR_PlayerArsenalLoadout PQD_CreateLoadoutSlot(SCR_Faction faction, string slotId, int slotIndex)
	{
		if (!faction)
			return null;
		
		// Get a base character prefab for this faction
		ResourceName characterPrefab = PQD_GetFactionBasePrefab(faction);
		if (characterPrefab.IsEmpty())
		{
			Print(string.Format("[PQD] No character prefab found for faction %1", faction.GetFactionKey()), LogLevel.WARNING);
			return null;
		}
		
		// Create the loadout instance
		SCR_PlayerArsenalLoadout loadout = new SCR_PlayerArsenalLoadout();
		
		// Set loadout properties
		loadout.m_sLoadoutName = string.Format("[PQD] Slot %1", slotIndex + 1);
		loadout.m_sLoadoutResource = characterPrefab;
		loadout.m_sAffiliatedFaction = faction.GetFactionKey();
		loadout.m_sPQDSlotId = slotId;
		
		return loadout;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get a base character prefab for a faction
	protected ResourceName PQD_GetFactionBasePrefab(SCR_Faction faction)
	{
		if (!faction)
			return "";
		
		string factionKey = faction.GetFactionKey();
		
		// Return appropriate character prefab based on faction
		// These are the official prefabs from CampaignLoadoutManager.et
		if (factionKey == "US")
			return "{BE7CA93E188A5B58}Prefabs/Characters/Campaign/Final/BLUFOR/US_army/Campaign_US_Base.et";
		else if (factionKey == "USSR")
			return "{918C557BF3506286}Prefabs/Characters/Campaign/Final/OPFOR/USSR_Army/Campaign_USSR_Base.et";
		else if (factionKey == "FIA")
			return "{BF1E43FF39AA526B}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Scout.et";
		else if (factionKey == "CIV")
			return "{99F600AD4C6BA6A4}Prefabs/Characters/Factions/CIV/Character_CIV_base.et";
		
		// For unknown factions, try to get a default character from the faction data
		Print(string.Format("[PQD] Unknown faction '%1', no default prefab available", factionKey), LogLevel.WARNING);
		return "";
	}
}
