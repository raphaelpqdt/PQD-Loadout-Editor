// PQD Loadout Editor - Arsenal Open Action
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
//! Action to open the PQD Loadout Editor from an arsenal
sealed class PQD_OpenLoadoutEditorAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Can add platform-specific restrictions here if needed
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[PQD] Failed to get MenuManager", LogLevel.ERROR);
			return;
		}
		
		PQD_LoadoutEditor menu = PQD_LoadoutEditor.Cast(menuManager.OpenMenu(ChimeraMenuPreset.PQD_LoadoutEditor));
		if (!menu)
		{
			Print("[PQD] Failed to open PQD_LoadoutEditor menu", LogLevel.ERROR);
			return;
		}
		
		menu.Init(pOwnerEntity, pUserEntity);
	}
}
