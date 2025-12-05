//------------------------------------------------------------------------------------------------
//! Data class for transferring loadout data from server to client
class PQD_LoadoutDataTransfer
{
	string factionKey;
	int slotIndex;
	string prefab;
	string loadoutData;
	float cost;
	string requiredRank;
}

//------------------------------------------------------------------------------------------------
sealed class PQD_PlayerControllerComponentClass : ScriptComponentClass {}

//------------------------------------------------------------------------------------------------
sealed class PQD_PlayerControllerComponent : ScriptComponent
{
	// Invokers for responses
	ref ScriptInvoker m_OnResponse_Storage = new ScriptInvoker();
	ref ScriptInvoker m_OnResponse_Loadout = new ScriptInvoker();
	
	// Components
	PQD_LoadoutStorageComponent m_LoadoutStorageComponent;
	SCR_MapMarkerEntrySquadLeader m_MapMarkerEntrySquadLeader;
	
	PlayerManager m_playerManager;
	SCR_BaseGameMode m_gameMode;
	PlayerController m_PC;
	
	static PQD_PlayerControllerComponent LocalInstance;
	static PQD_PlayerControllerComponent ServerInstance;
	
	protected SCR_ArsenalManagerComponent m_arsenalManager;

	static ref array<ref PQD_PlayerLoadout> AdminLoadoutMetadata = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
		m_PC = PlayerController.Cast(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (m_PC.GetPlayerId() == SCR_PlayerController.GetLocalPlayerId())
			LocalInstance = this;
		
		m_playerManager = GetGame().GetPlayerManager();
		
		if (!Replication.IsServer())
		{
			GetGame().GetCallqueue().CallLater(AskForLoadouts, 100, false);
			return;
		}
		
		if (SCR_PlayerController.GetLocalPlayerId() == 0)
			ServerInstance = this;
		
		m_gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!m_gameMode)
		{
			Print("[PQD] Failed to obtain game mode", LogLevel.WARNING);
			return;
		}

		m_LoadoutStorageComponent = PQD_LoadoutStorageComponent.Cast(GetGame().GetGameMode().FindComponent(PQD_LoadoutStorageComponent));
		UpdateServerLoadouts();
		
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerManager)
		{
			SCR_MapMarkerConfig markerConfig = markerManager.GetMarkerConfig();
			if (markerConfig)
				m_MapMarkerEntrySquadLeader = SCR_MapMarkerEntrySquadLeader.Cast(markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.SQUAD_LEADER));
		}
		
		SCR_ArsenalManagerComponent.GetArsenalManager(m_arsenalManager);
	}

	//------------------------------------------------------------------------------------------------
	void UpdateServerLoadouts()
	{
		AdminLoadoutMetadata.Clear();
		if (m_LoadoutStorageComponent)
			m_LoadoutStorageComponent.GetPlayerLoadoutMetadata(0, "", "admin", AdminLoadoutMetadata, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void AskForLoadouts()
	{
		Rpc(RpcAsk_LoadoutsPlease);
		
		// Also request valid slot data for deploy menu
		Rpc(RpcAsk_ValidSlotsPlease);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_ValidSlotsPlease()
	{
		// Get player ID and identity for this request
		int playerId = m_PC.GetPlayerId();
		string identityId = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		
		Print(string.Format("[PQD] Player %1 requesting valid slots, identity: %2", playerId, identityId), LogLevel.NORMAL);
		
		if (!m_LoadoutStorageComponent)
		{
			Print("[PQD] RpcAsk_ValidSlotsPlease: StorageComponent not found", LogLevel.WARNING);
			// Send empty response to mark cache as initialized
			Rpc(RpcDo_UpdateValidSlotsOwner, "{}");
			return;
		}
		
		// Get all playable factions
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			Print("[PQD] RpcAsk_ValidSlotsPlease: FactionManager not found", LogLevel.WARNING);
			Rpc(RpcDo_UpdateValidSlotsOwner, "{}");
			return;
		}
		
		array<Faction> factions = {};
		factionManager.GetFactionsList(factions);
		
		// Build response with valid slots per faction AND loadout data
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		
		ref map<string, ref array<int>> validSlotsMap = new map<string, ref array<int>>();
		
		// Also collect loadout data to send to client for preview
		// Format: array of { factionKey, slotIndex, prefab, loadoutData, cost, requiredRank }
		ref array<ref PQD_LoadoutDataTransfer> loadoutDataArray = new array<ref PQD_LoadoutDataTransfer>();
		
		foreach (Faction faction : factions)
		{
			SCR_Faction scrFaction = SCR_Faction.Cast(faction);
			if (!scrFaction || !scrFaction.IsPlayable())
				continue;
			
			string factionKey = scrFaction.GetFactionKey();
			ref array<int> validSlots = new array<int>();
			
			// Check each slot (0-4)
			for (int slotIndex = 0; slotIndex < 5; slotIndex++)
			{
				string prefab, loadoutData, requiredRank;
				float cost;
				
				if (m_LoadoutStorageComponent.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank))
				{
					validSlots.Insert(slotIndex);
					
					// Create transfer object with loadout data
					ref PQD_LoadoutDataTransfer transfer = new PQD_LoadoutDataTransfer();
					transfer.factionKey = factionKey;
					transfer.slotIndex = slotIndex;
					transfer.prefab = prefab;
					transfer.loadoutData = loadoutData;
					transfer.cost = cost;
					transfer.requiredRank = requiredRank;
					loadoutDataArray.Insert(transfer);
				}
			}
			
			validSlotsMap.Set(factionKey, validSlots);
			Print(string.Format("[PQD] Faction %1 has %2 valid slots", factionKey, validSlots.Count()), LogLevel.DEBUG);
		}
		
		saveContext.WriteValue("validSlots", validSlotsMap);
		saveContext.WriteValue("loadoutData", loadoutDataArray);
		string response = saveContext.ExportToString();
		
		Print(string.Format("[PQD] Sending valid slots and loadout data to player %1 (%2 loadouts)", playerId, loadoutDataArray.Count()), LogLevel.NORMAL);
		Rpc(RpcDo_UpdateValidSlotsOwner, response);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateValidSlotsOwner(string json)
	{
		Print(string.Format("[PQD] Received loadout data from server (size: %1 bytes)", json.Length()), LogLevel.NORMAL);
		
		// Parse the response
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		if (!loadContext.ImportFromString(json))
		{
			Print("[PQD] Failed to parse valid slots JSON", LogLevel.WARNING);
			PQD_ClientLoadoutCache.MarkInitialized();
			return;
		}
		
		// Read valid slots map
		ref map<string, ref array<int>> validSlotsMap = new map<string, ref array<int>>();
		loadContext.ReadValue("validSlots", validSlotsMap);
		
		// Update the client cache with valid slots
		foreach (string factionKey, array<int> slots : validSlotsMap)
		{
			PQD_ClientLoadoutCache.UpdateCache(factionKey, slots);
		}
		
		// Read and cache loadout data for preview
		ref array<ref PQD_LoadoutDataTransfer> loadoutDataArray = new array<ref PQD_LoadoutDataTransfer>();
		loadContext.ReadValue("loadoutData", loadoutDataArray);
		
		Print(string.Format("[PQD] Received %1 loadout data entries", loadoutDataArray.Count()), LogLevel.NORMAL);
		
		foreach (PQD_LoadoutDataTransfer transfer : loadoutDataArray)
		{
			if (transfer)
			{
				PQD_ClientLoadoutCache.SetLoadoutData(
					transfer.factionKey, 
					transfer.slotIndex, 
					transfer.prefab, 
					transfer.loadoutData, 
					transfer.cost, 
					transfer.requiredRank
				);
				Print(string.Format("[PQD] Cached loadout: %1 slot %2", transfer.factionKey, transfer.slotIndex), LogLevel.DEBUG);
			}
		}
		
		// Mark cache as initialized even if empty
		PQD_ClientLoadoutCache.MarkInitialized();
		
		Print("[PQD] Client loadout cache updated successfully with data", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_LoadoutsPlease()
	{
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", AdminLoadoutMetadata);
		
		string loadoutsJson = saveContext.ExportToString();
		
		Rpc(RpcDo_UpdateLoadoutsOwner, loadoutsJson);
	}
	
	//------------------------------------------------------------------------------------------------
	void BroadcastLoadoutChange()
	{
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", AdminLoadoutMetadata);
		
		string loadoutsJson = saveContext.ExportToString();
		Rpc(RpcDo_UpdateLoadoutsBroadcast, loadoutsJson);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateLoadoutsOwner(string json)
	{
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(json);
		loadContext.ReadValue("", AdminLoadoutMetadata);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_UpdateLoadoutsBroadcast(string json)
	{
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(json);
		loadContext.ReadValue("", AdminLoadoutMetadata);
	}
	
	//------------------------------------------------------------------------------------------------
	SCR_InventoryStorageManagerComponent GetPlayerInventoryManager(int playerId)
	{
		IEntity character = m_playerManager.GetPlayerControlledEntity(playerId);
		if (!character)
			return null;
		
		return SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	BaseInventoryStorageComponent GetPlayerStorage(PQD_StorageType storageType, int playerId)
	{
		IEntity character = m_playerManager.GetPlayerControlledEntity(playerId);
		
		if (storageType == PQD_StorageType.CHARACTER_LOADOUT)
			return BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		
		if (storageType == PQD_StorageType.CHARACTER_WEAPON)
			return BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		
		return null;
	}

	//------------------------------------------------------------------------------------------------
	void RequestAction(PQD_StorageRequest request)
	{
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", request);

		string requestString = saveContext.ExportToString();
		
		Print(string.Format("[PQD] Sending storage request: %1", requestString), LogLevel.DEBUG);
		Rpc(RpcAsk_RequestAction, requestString);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestAction(string requestJson)
	{
		int playerId = m_PC.GetPlayerId();
		
		PQD_StorageRequest request = new PQD_StorageRequest();
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(requestJson);
		loadContext.ReadValue("", request);
		
		Print(string.Format("[PQD] Processing request from player %1: %2", playerId, requestJson), LogLevel.DEBUG);
		
		// Handle visual identity change
		if (request.actionType == PQD_ActionType.CHANGE_VISUAL_IDENTITY)
		{
			HandleVisualIdentityChange(request, playerId);
			return;
		}
		
		Managed entity = Replication.FindItem(request.storageRplId);
		if (!entity)
		{
			SendActionResponse(request, false, "Invalid entity provided");
			return;
		}
		
		BaseInventoryStorageComponent editedStorage = BaseInventoryStorageComponent.Cast(entity);
		if (!editedStorage)
		{
			SendActionResponse(request, false, "Provided entity is not a storage component");
			return;
		}
		
		SCR_InventoryStorageManagerComponent storageManager = GetPlayerInventoryManager(playerId);
		if (!storageManager)
		{
			SendActionResponse(request, false, "Character storage manager not found");
			return;
		}

		switch (request.actionType)
		{
			case PQD_ActionType.ADD_ITEM:
				Action_AddItemToStorage(request, editedStorage, storageManager);
				break;
			case PQD_ActionType.REMOVE_ITEM:
				Action_TransferItemToStorage(request, editedStorage, storageManager);
				break;
			case PQD_ActionType.REPLACE_ITEM:
				Action_ReplaceItemInSlotWithPrefab(request, editedStorage, storageManager);
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleVisualIdentityChange(PQD_StorageRequest request, int playerId)
	{
		IEntity ent = m_PC.GetControlledEntity();
		if (!ent)
		{
			SendActionResponse(request, false, "Could not find controlled entity");
			return;
		}

		IEntity arsenalEntity = PQD_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity)
		{
			SendActionResponse(request, false, "Could not find arsenal entity");
			return;
		}
		
		SCR_FactionAffiliationComponent arsenalFactionComp = SCR_FactionAffiliationComponent.Cast(arsenalEntity.FindComponent(SCR_FactionAffiliationComponent));
		if (!arsenalFactionComp)
		{
			SendActionResponse(request, false, "Could not find faction component");
			return;
		}
		
		Faction arsenalFaction = arsenalFactionComp.GetAffiliatedFaction();
		if (!arsenalFaction)
		{
			SendActionResponse(request, false, "Could not find arsenal faction");
			return;
		}
		
		FactionIdentity factionIdentity = arsenalFaction.GetFactionIdentity();
		if (!factionIdentity)
		{
			SendActionResponse(request, false, "Faction has no identity");
			return;
		}

		SCR_CharacterIdentityComponent identityComponent = SCR_CharacterIdentityComponent.Cast(ent.FindComponent(SCR_CharacterIdentityComponent));
		if (!identityComponent)
		{
			SendActionResponse(request, false, "No identity component on character");
			return;
		}
		
		Identity id = identityComponent.GetIdentity();
		VisualIdentity newVisualIdentity = factionIdentity.CreateVisualIdentity(request.storageSlotId);
		if (!newVisualIdentity)
		{
			SendActionResponse(request, false, "Creation of Visual Identity failed");
			return;
		}
		id.SetVisualIdentity(newVisualIdentity);
		
		identityComponent.SetIdentity(id);
		SendActionResponse(request, true, "Identity changed");
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanAffordItem(PQD_StorageRequest request, out float cost)
	{
		if (!SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled())
			return true;

		IEntity arsenalEntity = PQD_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity)
		{
			SendActionResponse(request, false, "Cannot find Arsenal Entity");
			return false;
		}

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent)
		{
			SendActionResponse(request, false, "Cannot find Arsenal Component");
			return false;
		}
		
		cost = PQD_Helpers.GetItemSupplyCost(arsenalComponent, request.prefab);
		if (cost < 0.1)
			return true;

		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (!resourceComponent)
		{
			SendActionResponse(request, false, "Cannot find Resource Component");
			return false;
		}
		
		SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer)
		{
			SendActionResponse(request, false, "Cannot find Resource Consumer");
			return false;
		}
		
		cost *= consumer.GetBuyMultiplier();
		SCR_ResourceConsumtionResponse resp = consumer.RequestConsumtion(cost);
		bool success = resp.GetReason() == EResourceReason.SUFFICIENT;
		
		if (!success)
			SendActionResponse(request, false, string.Format("Not enough supplies (cost: %1)", cost));
		
		return success;
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasHighEnoughRank(PQD_StorageRequest request)
	{
		if (m_arsenalManager && !m_arsenalManager.AreItemsRankLocked())
			return true;

		IEntity arsenalEntity = PQD_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity)
		{
			SendActionResponse(request, false, "Cannot find Arsenal Entity");
			return false;
		}
		
		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent)
		{
			SendActionResponse(request, false, "Cannot find Arsenal Component");
			return false;
		}

		SCR_ECharacterRank playerRank = SCR_CharacterRankComponent.GetCharacterRank(SCR_PlayerController.Cast(GetOwner()).GetControlledEntity());
		SCR_ECharacterRank rankRequired = PQD_Helpers.GetItemRequiredRank(arsenalComponent, request.prefab);

		if (playerRank == SCR_ECharacterRank.INVALID || rankRequired == SCR_ECharacterRank.INVALID)
			return true;
		
		if (playerRank < rankRequired)
		{
			string playerAsString = typename.EnumToString(SCR_ECharacterRank, playerRank);
			playerAsString.ToLower();
			string requiredAsString = typename.EnumToString(SCR_ECharacterRank, rankRequired);
			requiredAsString.ToLower();
			
			SendActionResponse(request, false, string.Format("%1 rank required (you are %2)", requiredAsString, playerAsString));
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_AddItemToStorage(PQD_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager)
	{
		if (!HasHighEnoughRank(request))
			return;
		
		IEntity itemEntity = PQD_Helpers.PrepareTemporaryEntityAtCoords(request.prefab, storageManager.GetOwner().GetOrigin());
		if (!itemEntity)
		{
			SendActionResponse(request, false, "Failed to spawn temporary entity");
			return;
		}

		BaseInventoryStorageComponent appropriateStorage = storageManager.FindStorageForInsert(itemEntity, storage, EStoragePurpose.PURPOSE_ANY);
		if (!appropriateStorage)
		{
			SendActionResponse(request, false, "Failed to find suitable storage");
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
			return;
		}
		
		float cost;
		if (!CanAffordItem(request, cost))
			return;
		
		string messageOk = "Item added";
		if (cost > 0)
			messageOk = string.Format("%1 (cost: %2 supply)", messageOk, cost);
		
		PQD_InvCallback_DeleteOnFail deleteCb = new PQD_InvCallback_DeleteOnFail();
		deleteCb.messageOk = messageOk;
		deleteCb.messageFailed = "Failed to add item to storage";
		deleteCb.temporaryEntity = itemEntity;
		deleteCb.component = this;
		deleteCb.request = request;

		storageManager.TryInsertItemInStorage(itemEntity, appropriateStorage, -1, deleteCb);
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool TryRefundItem(PQD_StorageRequest request, IEntity attachedEntity, out float refund)
	{
		IEntity arsenalEntity = PQD_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity)
		{
			SendActionResponse(request, false, "Invalid arsenal entity");
			return false;
		}

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent)
		{
			SendActionResponse(request, false, "Invalid arsenal component");
			return false;
		}
		
		refund = Math.Clamp(SCR_ArsenalManagerComponent.GetItemRefundAmount(attachedEntity, arsenalComponent, false), 0, float.MAX);
		
		InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(attachedEntity.FindComponent(InventoryItemComponent));
		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (resourceComponent)
		{
			auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
			resourceInventoryComponent.RpcAsk_ArsenalRefundItem(Replication.FindId(resourceComponent), Replication.FindId(inventoryItemComponent), EResourceType.SUPPLIES);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void TryRefundFixedCost(IEntity arsenalEntity, float cost)
	{
		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (!resourceComponent)
			return;
		
		SCR_ResourceGenerator generator = resourceComponent.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!generator)
			return;
		
		auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
		if (!resourceInventoryComponent)
			return;
		
		// TODO: Implement refund method
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_TransferItemToStorage(PQD_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager)
	{
		InventoryStorageSlot slot = storage.GetSlot(request.storageSlotId);
		if (!slot)
		{
			SendActionResponse(request, false, "Requested slot does not exist");
			return;
		}

		IEntity attachedEntity = slot.GetAttachedEntity();
  		if (!attachedEntity)
		{
			SendActionResponse(request, false, "Provided entity is invalid");
			return;
		}

		float refund;
		if (!TryRefundItem(request, attachedEntity, refund))
			return;

		if (!attachedEntity)
		{
			string confirmation = "Item removed";
			if (refund > 0.1)
				confirmation = string.Format("%1 (+%2 supply)", confirmation, refund);
			SendActionResponse(request, true, confirmation);
			return;
		}
		
		PQD_InvCallback cb = new PQD_InvCallback();
		cb.messageOk = "Item removed";
		cb.messageFailed = "Failed to remove item from storage";
		cb.request = request;
		cb.component = this;

		storageManager.TryDeleteItem(attachedEntity, cb);
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_ReplaceItemInSlotWithPrefab(PQD_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager)
	{
		InventoryStorageSlot slot = storage.GetSlot(request.storageSlotId);
		if (!slot)
		{
			SendActionResponse(request, false, "Requested slot does not exist");
			return;
		}
		
		if (!HasHighEnoughRank(request))
			return;

		float cost;
		if (!CanAffordItem(request, cost))
			return;
			
		string messageOk = "Item added";
		if (cost > 0)
			messageOk = string.Format("%1 (-%2 supply)", messageOk, cost);
		
		IEntity attachedEntity = slot.GetAttachedEntity();

		float refund = 0;
		if (attachedEntity)
		{
			if (!TryRefundItem(request, attachedEntity, refund))
				return;
		}
		if (refund > 0)
			messageOk = string.Format("%1 (+%2 refund)", messageOk, refund);
		
  		if (!attachedEntity)
		{
			IEntity itemEntity = PQD_Helpers.PrepareTemporaryEntityAtCoords(request.prefab, storageManager.GetOwner().GetOrigin());
			if (!itemEntity)
			{
				SendActionResponse(request, false, "Failed to spawn temporary entity");
				return;
			}

			PQD_InvCallback_DeleteOnFail deleteCb = new PQD_InvCallback_DeleteOnFail();
			deleteCb.messageOk = messageOk;
			deleteCb.messageFailed = "Failed to add item to storage";
			deleteCb.temporaryEntity = itemEntity;
			deleteCb.component = this;
			deleteCb.request = request;

			storageManager.TryInsertItemInStorage(itemEntity, storage, request.storageSlotId, deleteCb);
		}
		else
		{
			PQD_InvCallback_SpawnAfterDelete deleteCb = new PQD_InvCallback_SpawnAfterDelete();
			deleteCb.request = request;
			deleteCb.component = this;
			deleteCb.storageManager = storageManager;
			deleteCb.slotStorage = storage;
	
			storageManager.TryDeleteItem(attachedEntity, deleteCb);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SendActionResponse(PQD_NetworkRequest request, bool success, string message = "")
	{
		Print(string.Format("[PQD] Response: %1, success: %2, message: %3", request.Repr(), success, message), LogLevel.DEBUG);
		
		PQD_NetworkResponse response = new PQD_NetworkResponse();
		response.request = request;
		response.success = success;
		response.message = message;
		
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", response);
		
		string responseString = saveContext.ExportToString();
		
		Rpc(RpcDo_SendActionResponse, responseString);
	}
	
	//------------------------------------------------------------------------------------------------
	// Loadout operations
	void RequestLoadoutAction(PQD_LoadoutRequest request)
	{
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", request);

		string requestString = saveContext.ExportToString();
		
		Print(string.Format("[PQD] Sending loadout request: %1", requestString), LogLevel.DEBUG);
		Rpc(RpcAsk_RequestLoadoutAction, requestString);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestLoadoutAction(string requestJson)
	{
		int playerId = m_PC.GetPlayerId();

		PQD_LoadoutRequest request = new PQD_LoadoutRequest();
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(requestJson);
		loadContext.ReadValue("", request);
		
		if (!m_LoadoutStorageComponent)
		{
			SendActionResponse(request, false, "Loadout manager component missing from game mode");
			return;
		}

		SCR_ArsenalManagerComponent arsenalManager;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager))
		{
			SendActionResponse(request, false, "No Arsenal Manager found");
			return;
		}
		
		Print(string.Format("[PQD] Processing loadout request from player %1: %2", playerId, requestJson), LogLevel.DEBUG);
		
		string identity;
		if (!PQD_Helpers.GetPlayerIdentityId(playerId, identity))
		{
			SendActionResponse(request, false, "Invalid player identity");
			return;
		}
		
		string factionKey;
		if (!PQD_Helpers.GetPlayerEntityFactionKey(playerId, factionKey))
		{
			SendActionResponse(request, false, "Invalid faction");
			return;
		}
		
		Managed entity = Replication.FindItem(request.arsenalComponentRplId);
		if (!entity)
		{
			SendActionResponse(request, false, "Invalid Arsenal Component");
			return;
		}
		
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity);
		if (!entity)
		{
			SendActionResponse(request, false, "RplId is not an Arsenal Component");
			return;
		}

		switch (request.actionType)
		{
			case PQD_ActionType.GET_LOADOUTS:
				Action_GetLoadoutList(request, identity, factionKey, playerId);
				break;
			case PQD_ActionType.SAVE_LOADOUT:
				Action_SavePlayerLoadout(request, arsenalManager, arsenal, identity, factionKey, playerId);
				break;
			case PQD_ActionType.APPLY_LOADOUT:
				Action_ApplyPlayerLoadout(arsenal, arsenalManager, request, identity, factionKey, playerId);
				break;
			case PQD_ActionType.CLEAR_LOADOUT:
				Action_ClearPlayerLoadout(request, identity, factionKey, playerId);
				break;
			case PQD_ActionType.GET_ADMIN_LOADOUTS:
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId()))
				{
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_GetLoadoutList(request, identity, factionKey, playerId, true);
				break;
			case PQD_ActionType.SAVE_LOADOUT_ADMIN:
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId()))
				{
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_SavePlayerLoadout(request, arsenalManager, arsenal, identity, factionKey, playerId, true);
				
				if (ServerInstance)
				{
					ServerInstance.UpdateServerLoadouts();
					ServerInstance.BroadcastLoadoutChange();
				}
				break;
			case PQD_ActionType.APPLY_LOADOUT_ADMIN:
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId()))
				{
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_ApplyPlayerLoadout(arsenal, arsenalManager, request, identity, factionKey, playerId, true);
				break;
			case PQD_ActionType.CLEAR_LOADOUT_ADMIN:
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId()))
				{
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_ClearPlayerLoadout(request, identity, factionKey, playerId, true);
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_GetLoadoutList(PQD_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false)
	{
		array<ref PQD_PlayerLoadout> loadoutOptions = {};

		m_LoadoutStorageComponent.GetPlayerLoadoutMetadata(playerId, identity, factionKey, loadoutOptions, isAdminLoadout);

		// Log what we're about to serialize
		foreach (PQD_PlayerLoadout loadout : loadoutOptions)
		{
			Print(string.Format("[PQD] Action_GetLoadoutList: Slot %1, HasData=%2, prefab='%3'",
				loadout.slotId, loadout.HasData(), loadout.prefab), LogLevel.NORMAL);
		}

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("loadouts", loadoutOptions);

		string json = ctx.ExportToString();
		Print(string.Format("[PQD] Action_GetLoadoutList: Serialized JSON length: %1", json.Length()), LogLevel.NORMAL);

		SendActionResponse(request, true, json);
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_SavePlayerLoadout(PQD_LoadoutRequest request, SCR_ArsenalManagerComponent arsenalManager, SCR_ArsenalComponent arsenal, string identity, string factionKey, int playerId, bool isAdminLoadout = false)
	{
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlledEntity)
			return;

		// Check if player can save loadout according to arsenal configuration (faction restrictions, etc.)
		// Admin loadouts bypass this check
		if (!isAdminLoadout)
		{
			FactionAffiliationComponent factionAffiliation = FactionAffiliationComponent.Cast(controlledEntity.FindComponent(FactionAffiliationComponent));
			if (!arsenalManager.PQD_CanSaveLoadout(playerId, GameEntity.Cast(controlledEntity), factionAffiliation, arsenal, true))
			{
				SendActionResponse(request, false, "Cannot save loadout: Arsenal restrictions (invalid faction items?)");
				return;
			}
		}

		if (!m_LoadoutStorageComponent.SaveCurrentPlayerLoadout(arsenalManager, playerId, identity, controlledEntity, factionKey, request.loadoutSlotId, isAdminLoadout))
		{
			SendActionResponse(request, false, "Loadout saving failed");
			return;
		}
		
		// Notify client to update their slot cache (for deploy menu visibility)
		if (!isAdminLoadout)
			NotifyClientSlotUpdate(playerId, factionKey, request.loadoutSlotId, true);
		
		Action_GetLoadoutList(request, identity, factionKey, playerId, isAdminLoadout);
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_ClearPlayerLoadout(PQD_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false)
	{
		if (!m_LoadoutStorageComponent.ClearLoadoutSlot(playerId, identity, factionKey, request.loadoutSlotId, isAdminLoadout))
		{
			SendActionResponse(request, false, "Failed to clear loadout slot");
			return;
		}
		
		// Notify client to update their slot cache (for deploy menu visibility)
		if (!isAdminLoadout)
			NotifyClientSlotUpdate(playerId, factionKey, request.loadoutSlotId, false);
		
		Action_GetLoadoutList(request, identity, factionKey, playerId, isAdminLoadout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Notify the client about a slot validity change (save or clear)
	void NotifyClientSlotUpdate(int playerId, string factionKey, int slotIndex, bool isValid)
	{
		Print(string.Format("[PQD] Notifying client about slot update: player=%1, faction=%2, slot=%3, valid=%4", 
			playerId, factionKey, slotIndex, isValid), LogLevel.DEBUG);
		
		if (isValid && m_LoadoutStorageComponent)
		{
			// If slot is now valid, also send the loadout data for preview
			string prefab, loadoutData, requiredRank;
			float cost;
			
			if (m_LoadoutStorageComponent.GetPlayerLoadoutData(playerId, factionKey, slotIndex, prefab, loadoutData, cost, false, requiredRank))
			{
				// Send slot update with data
				Rpc(RpcDo_UpdateSingleSlotWithDataOwner, factionKey, slotIndex, isValid, prefab, loadoutData, cost, requiredRank);
				return;
			}
		}
		
		// Fallback: just send validity update without data
		Rpc(RpcDo_UpdateSingleSlotOwner, factionKey, slotIndex, isValid);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateSingleSlotWithDataOwner(string factionKey, int slotIndex, bool isValid, string prefab, string loadoutData, float cost, string requiredRank)
	{
		Print(string.Format("[PQD] Client received slot update with data: faction=%1, slot=%2, valid=%3, data size=%4", 
			factionKey, slotIndex, isValid, loadoutData.Length()), LogLevel.NORMAL);
		
		if (isValid)
		{
			// Update the client cache with the new data
			PQD_ClientLoadoutCache.SetLoadoutData(factionKey, slotIndex, prefab, loadoutData, cost, requiredRank);
			
			// Also update the valid slots array
			// Request full refresh to update the valid slots properly
			Rpc(RpcAsk_ValidSlotsPlease);
		}
		else
		{
			// Clear the data for this slot
			PQD_ClientLoadoutCache.ClearLoadoutData(factionKey, slotIndex);
			Rpc(RpcAsk_ValidSlotsPlease);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateSingleSlotOwner(string factionKey, int slotIndex, bool isValid)
	{
		Print(string.Format("[PQD] Client received slot update: faction=%1, slot=%2, valid=%3", factionKey, slotIndex, isValid), LogLevel.DEBUG);
		
		// Update the client cache
		// First, check if we have data for this faction
		// We need to get the current slots and modify them
		
		// For simplicity, request a full refresh of valid slots
		// This ensures consistency with the server
		Rpc(RpcAsk_ValidSlotsPlease);
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_ApplyPlayerLoadout(SCR_ArsenalComponent arsenal, SCR_ArsenalManagerComponent arsenalManager, PQD_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false)
	{
		IEntity previousEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!previousEntity)
			return;
		
		string loadoutData;
		string prefab;
		string requiredRank;
		float cost;
		
		if (!m_LoadoutStorageComponent.GetPlayerLoadoutData(playerId, factionKey, request.loadoutSlotId, prefab, loadoutData, cost, isAdminLoadout, requiredRank))
		{
			SendActionResponse(request, false, "Failed to load loadout");
			return;
		}
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		previousEntity.GetWorldTransform(params.Transform);
		params.Transform[3] = params.Transform[3] + (vector.Up * 0.05);
		
		if (prefab.IsEmpty())
		{
			SendActionResponse(request, false, "Invalid character prefab");
			return;
		}
		
		Resource loaded = Resource.Load(prefab);
		if (!loaded)
		{
			SendActionResponse(request, false, "Failed to load character prefab");
			return;
		}
		
		IEntity entity = GameEntity.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), params));
		if (!entity)
		{
			SendActionResponse(request, false, "Spawning character entity failed");
			return;
		}
		
		SCR_ChimeraAIAgent agent = PQD_Helpers.FindAIAgent(entity);
		if (agent)
			agent.SetPlayerPending_S(playerId);
		
		SCR_EditableCharacterComponent editorCharacter = SCR_EditableCharacterComponent.Cast(entity.FindComponent(SCR_EditableCharacterComponent));
		if (editorCharacter)
			editorCharacter.SetIsPlayerPending(playerId);
		
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (!controller)
		{
			RplComponent.DeleteRplEntity(entity, false);
			SendActionResponse(request, false, "Spawned entity has no Character Controller");
			return;
		}
		
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
		
		GetGame().GetCallqueue().CallLater(Action_ApplyPlayerLoadout_StepTwo, 200, false, arsenal.GetOwner(), loadoutData, controller, entity, previousEntity, request, playerId, cost);
	}
	
	//------------------------------------------------------------------------------------------------
	void Action_ApplyPlayerLoadout_StepTwo(IEntity arsenal, string loadoutData, CharacterControllerComponent controller, IEntity newEntity, IEntity previousEntity, PQD_LoadoutRequest request, int playerId, float refundCost)
	{
		string identity;
		PQD_Helpers.GetPlayerIdentityId(playerId, identity);
		Print(string.Format("[PQD] Loading loadout for player id: %1, identity: %2", playerId, identity), LogLevel.NORMAL);
		
		if (!newEntity || !previousEntity || !controller)
		{
			Print("[PQD] Entity disappeared before loadout could be applied", LogLevel.ERROR);
			TryRefundFixedCost(arsenal, refundCost);
			return;
		}

		GameEntity entityGame = GameEntity.Cast(newEntity);

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		ctx.ImportFromString(loadoutData);
		
		SCR_PlayerArsenalLoadout.ApplyLoadoutString(entityGame, ctx);
		
		SCR_ECharacterRank rank = SCR_CharacterRankComponent.GetCharacterRank(previousEntity);
		if (rank != SCR_ECharacterRank.INVALID)
		{
			SCR_CharacterRankComponent comp = SCR_CharacterRankComponent.Cast(newEntity.FindComponent(SCR_CharacterRankComponent));
			if (comp)
				comp.SetCharacterRank(rank, true);
		}

		SCR_PlayerController.Cast(GetOwner()).SetInitialMainEntity(newEntity);
		
		SendActionResponse(request, true, "Loadout applied");
		RplComponent.DeleteRplEntity(previousEntity, false);
		
		AfterLoadoutAppliedSuccessfully(playerId, newEntity);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void AfterLoadoutAppliedSuccessfully(int playerId, IEntity newEntity)
	{
		SCR_PerceivedFactionManagerComponent perceivedFactionManager = SCR_PerceivedFactionManagerComponent.GetInstance();
		if (perceivedFactionManager)
			perceivedFactionManager.OnPlayerSpawnFinalize_S(null, null, null, newEntity);

		if (m_gameMode)
			m_gameMode.GetOnPlayerSpawned().Invoke(playerId, newEntity);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_SendActionResponse(string responseJson)
	{
		Print(string.Format("[PQD] Processing response: %1", responseJson), LogLevel.DEBUG);
		
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(responseJson);
		
		PQD_NetworkResponse response = new PQD_NetworkResponse();
		loadContext.ReadValue("", response);

		switch (response.request.actionType)
		{
			case PQD_ActionType.ADD_ITEM:
			case PQD_ActionType.REMOVE_ITEM:
			case PQD_ActionType.REPLACE_ITEM: 
			case PQD_ActionType.CHANGE_VISUAL_IDENTITY: 
			case PQD_ActionType.CHANGE_SOUND_IDENTITY:
				PQD_StorageRequest storageRequest;
				loadContext.ReadValue("request", storageRequest);
				m_OnResponse_Storage.Invoke(response, storageRequest);
				break;
			case PQD_ActionType.GET_ADMIN_LOADOUTS:
			case PQD_ActionType.SAVE_LOADOUT_ADMIN:
			case PQD_ActionType.APPLY_LOADOUT_ADMIN:
			case PQD_ActionType.CLEAR_LOADOUT_ADMIN:
			case PQD_ActionType.GET_LOADOUTS:
			case PQD_ActionType.SAVE_LOADOUT:
			case PQD_ActionType.CLEAR_LOADOUT:
			case PQD_ActionType.APPLY_LOADOUT:
				PQD_LoadoutRequest loadoutRequest;
				loadContext.ReadValue("request", loadoutRequest);
				m_OnResponse_Loadout.Invoke(response, loadoutRequest);
				break;
		}
	}
}

//------------------------------------------------------------------------------------------------
// Inventory operation callbacks
class PQD_InvCallback : ScriptedInventoryOperationCallback
{
	PQD_NetworkRequest request;
	string messageOk;
	string messageFailed;
	PQD_PlayerControllerComponent component;

	override void OnComplete()
	{
		if (component)
			component.SendActionResponse(request, true, messageOk);
	}
	
	override void OnFailed()
	{
		if (component)
			component.SendActionResponse(request, false, messageFailed);
	}
}

//------------------------------------------------------------------------------------------------
sealed class PQD_InvCallback_DeleteOnFail : PQD_InvCallback
{
	IEntity temporaryEntity;
	
	override void OnFailed()
	{
		SCR_EntityHelper.DeleteEntityAndChildren(temporaryEntity);
		super.OnFailed();
	}
}

//------------------------------------------------------------------------------------------------
sealed class PQD_InvCallback_SpawnAfterDelete : PQD_InvCallback
{
	SCR_InventoryStorageManagerComponent storageManager;
	BaseInventoryStorageComponent slotStorage;
	
	override void OnComplete()
	{
		PQD_StorageRequest storageRequest = PQD_StorageRequest.Cast(request);
		
		if (storageRequest.prefab != "empty")
		{
			IEntity itemEntity = PQD_Helpers.PrepareTemporaryEntityAtCoords(storageRequest.prefab, storageManager.GetOwner().GetOrigin());
			if (!itemEntity)
			{
				messageFailed = string.Format("Failed to create entity from prefab: %1", storageRequest.prefab);
				OnFailed();
				return;
			}

			PQD_InvCallback_DeleteOnFail deleteCb = new PQD_InvCallback_DeleteOnFail();
			deleteCb.messageOk = "Item added";
			deleteCb.messageFailed = "Failed to add item to storage";
			deleteCb.temporaryEntity = itemEntity;
			deleteCb.component = component;
			deleteCb.request = request;
	
			storageManager.TryInsertItemInStorage(itemEntity, slotStorage, storageRequest.storageSlotId, deleteCb);
			return;
		}
		
		messageOk = "Item removed";
		super.OnComplete();
	}
	
	override void OnFailed()
	{
		if (messageFailed.IsEmpty())
		{
			PQD_StorageRequest storageRequest = PQD_StorageRequest.Cast(request);
			messageFailed = string.Format("Failed to delete item %1 in storage %2", storageRequest.storageSlotId, slotStorage.Type());
		}
		
		super.OnFailed();
	}
}
