// PQD Loadout Editor - GameMode Component
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
//! GameMode component that enables PQD Loadout Editor functionality
//! This is an optional component - if not present, default values are used
[ComponentEditorProps(category: "PQD/GameMode", description: "PQD Loadout Editor GameMode Component")]
class PQD_GameModeComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
class PQD_GameModeComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable loadout saving/loading functionality")]
	protected bool m_bEnableLoadouts;
	
	[Attribute("5", UIWidgets.Slider, "Maximum number of loadout slots per player", "1 20 1")]
	protected int m_iMaxLoadoutSlots;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable admin loadouts that can be applied to all players")]
	protected bool m_bEnableAdminLoadouts;
	
	[Attribute("$profile:/PQDLoadoutEditor_Loadouts/", UIWidgets.EditBox, "Base path for loadout save files")]
	protected string m_sLoadoutSavePath;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable supply cost system")]
	protected bool m_bEnableSupplyCost;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable rank restrictions")]
	protected bool m_bEnableRankRestrictions;
	
	protected static PQD_GameModeComponent s_Instance;
	
	//------------------------------------------------------------------------------------------------
	static PQD_GameModeComponent GetInstance()
	{
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		
		Print("[PQD] GameMode Component initialized", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnGameEnd()
	{
		super.OnGameEnd();
		s_Instance = null;
	}
	
	//------------------------------------------------------------------------------------------------
	bool AreLoadoutsEnabled()
	{
		return m_bEnableLoadouts;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetMaxLoadoutSlots()
	{
		return m_iMaxLoadoutSlots;
	}
	
	//------------------------------------------------------------------------------------------------
	bool AreAdminLoadoutsEnabled()
	{
		return m_bEnableAdminLoadouts;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetLoadoutSavePath()
	{
		return m_sLoadoutSavePath;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsSupplyCostEnabled()
	{
		return m_bEnableSupplyCost;
	}
	
	//------------------------------------------------------------------------------------------------
	bool AreRankRestrictionsEnabled()
	{
		return m_bEnableRankRestrictions;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Validate if a player can use a specific loadout
	bool CanPlayerUseLoadout(int playerId, PQD_PlayerLoadout loadout)
	{
		if (!loadout)
			return false;
		
		// Check rank restriction
		if (m_bEnableRankRestrictions)
		{
			SCR_ECharacterRank requiredRank = typename.StringToEnum(SCR_ECharacterRank, loadout.required_rank);
			
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (playerEntity)
			{
				SCR_ECharacterRank playerRank = SCR_CharacterRankComponent.GetCharacterRank(playerEntity);
				if (playerRank < requiredRank)
					return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get player's current supply points
	int GetPlayerSupplyPoints(int playerId)
	{
		// This would integrate with whatever supply/economy system exists
		return 1000;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Deduct supply points from player
	bool DeductPlayerSupplyPoints(int playerId, int amount)
	{
		if (!m_bEnableSupplyCost)
			return true;
		
		int current = GetPlayerSupplyPoints(playerId);
		if (current < amount)
			return false;
		
		// Deduct points (would integrate with economy system)
		return true;
	}
}
