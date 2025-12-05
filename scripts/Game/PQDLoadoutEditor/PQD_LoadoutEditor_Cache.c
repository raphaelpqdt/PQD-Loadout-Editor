// PQD Loadout Editor - Cache System
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
sealed class PQD_Cache
{
	private static PQD_Cache m_Instance;
	static PQD_Cache GetInstance() { return m_Instance; }

	private ref map<RplId, BaseInventoryStorageComponent> m_RplId_Storage = new map<RplId, BaseInventoryStorageComponent>();
	private ref map<BaseInventoryStorageComponent, RplId> m_Storage_RplId = new map<BaseInventoryStorageComponent, RplId>();
	private ref map<BaseInventoryStorageComponent, PQD_StorageType> m_Storage_StorageType = new map<BaseInventoryStorageComponent, PQD_StorageType>();
	
	// Cache all arsenal items
	private ref array<SCR_ArsenalItem> m_ArsenalItems = {};
	private ref set<ResourceName> m_ArsenalItemsKnownSubArsenals = new set<ResourceName>();
	private ref set<ResourceName> m_ArsenalItemsKnownSubItems = new set<ResourceName>();
	
	// Cache available prefabs per slot type
	private ref map<string, ref array<ResourceName>> m_SlotOptions = new map<string, ref array<ResourceName>>();
	private ref map<string, ref array<SCR_ArsenalItem>> m_ArsenalItemTypes = new map<string, ref array<SCR_ArsenalItem>>();
	
	private ref map<ResourceName, float> m_ArsenalItemCost = new map<ResourceName, float>;
	private ref map<ResourceName, ref PQD_ArsenalItemDetails> m_ArsenalItemDetails = new map<ResourceName, ref PQD_ArsenalItemDetails>;
	
	private bool m_bAreItemsRankLocked = true;
	
	private SCR_ArsenalComponent m_CurrentArsenalComponent;
	private SCR_ResourceComponent m_ArsenalResourceComponent;
	
	//------------------------------------------------------------------------------------------------
	SCR_ArsenalComponent GetArsenalComponent() { return m_CurrentArsenalComponent; }
	SCR_ResourceComponent GetArsenalResourceComponent() { return m_ArsenalResourceComponent; }
	
	//------------------------------------------------------------------------------------------------
	bool Init(SCR_ArsenalComponent arsenalComponent)
	{
		m_Instance = null;

		bool success = arsenalComponent.GetFilteredArsenalItems(m_ArsenalItems);
		Print(string.Format("[PQD] Cache Init: Got %1 arsenal items, success=%2", m_ArsenalItems.Count(), success), LogLevel.NORMAL);
		m_CurrentArsenalComponent = arsenalComponent;
		
		m_ArsenalResourceComponent = SCR_ResourceComponent.Cast(SCR_EntityHelper.FindComponent(arsenalComponent.GetOwner(), SCR_ResourceComponent));
		
		if (success)
		{
			m_Instance = this;
			
			SCR_ArsenalManagerComponent arsenalManager;
			if (SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager))
				m_bAreItemsRankLocked = arsenalManager.AreItemsRankLocked();

			SCR_EArsenalSupplyCostType costType = arsenalComponent.GetSupplyCostType();

			foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
			{
				PQD_ArsenalItemDetails details = new PQD_ArsenalItemDetails;
				details.supplyCost = arsenalItem.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT);
				details.requiredRank = arsenalItem.GetRequiredRank();
				
				m_ArsenalItemDetails.Set(arsenalItem.GetItemResourceName(), details);
			}
		}
		
		return success;
	}

	//------------------------------------------------------------------------------------------------
	bool AreSuppliesEnabled()
	{
		return PQD_Helpers.AreSuppliesEnabledForComponent(m_ArsenalResourceComponent);
	}
	
	//------------------------------------------------------------------------------------------------
	float GetArsenalItemCost(ResourceName prefab)
	{
		return m_ArsenalItemCost.Get(prefab);
	}
	
	//------------------------------------------------------------------------------------------------
	void GetArsenalItemCostAndRank(ResourceName prefab, out float cost, out SCR_ECharacterRank rank)
	{
		PQD_ArsenalItemDetails details = m_ArsenalItemDetails.Get(prefab);
		if (!details)
		{
			cost = 0;
			rank = SCR_ECharacterRank.INVALID;
			return;
		}
		
		cost = details.supplyCost;
		
		if (m_bAreItemsRankLocked)
			rank = details.requiredRank;
		else
			rank = SCR_ECharacterRank.INVALID;
	}
	
	//------------------------------------------------------------------------------------------------
	void GetArsenalItemRank(ResourceName prefab, out SCR_ECharacterRank rank)
	{
		PQD_ArsenalItemDetails details = m_ArsenalItemDetails.Get(prefab);
		if (!details)
		{
			rank = SCR_ECharacterRank.INVALID;
			return;
		}
		
		rank = details.requiredRank;
	}
	
	//------------------------------------------------------------------------------------------------
	RplId GetStorageRplId(BaseInventoryStorageComponent comp)
	{
		if (m_Storage_RplId.Contains(comp))
			return m_Storage_RplId.Get(comp);
		
		RplId rplId = Replication.FindId(comp);
		m_RplId_Storage.Set(rplId, comp);
		m_Storage_RplId.Set(comp, rplId);

		return rplId;
	}
	
	//------------------------------------------------------------------------------------------------
	BaseInventoryStorageComponent GetStorageByRplId(RplId rplId)
	{
		return m_RplId_Storage.Get(rplId);
	}
	
	//------------------------------------------------------------------------------------------------
	PQD_StorageType GetStorageType(BaseInventoryStorageComponent comp)
	{
		if (m_Storage_StorageType.Contains(comp))
			return m_Storage_StorageType.Get(comp);
		
		PQD_StorageType storageType = PQD_Helpers.GetStorageType(comp);
		m_Storage_StorageType.Set(comp, storageType);
		
		return storageType;
	}
	
	//------------------------------------------------------------------------------------------------
	bool TryGetPrefabsFromCache(string cacheKey, out array<ResourceName> outValidPrefabs, out int outItems)
	{
		if (!m_SlotOptions.Contains(cacheKey))
			return false;
		
		array<ResourceName> choices = m_SlotOptions.Get(cacheKey);
		foreach (ResourceName choice: choices)
		{
			outValidPrefabs.Insert(choice);
		}
		
		outItems = choices.Count();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool TryGetArsenalItemsFromCache(string cacheKey, out array<SCR_ArsenalItem> outValidItems, out int outItems)
	{
		if (!m_ArsenalItemTypes.Contains(cacheKey))
			return false;
		
		outValidItems = m_ArsenalItemTypes.Get(cacheKey);
		outItems = outValidItems.Count();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetPrefabsForLoadoutAreaTypeSlot(PQD_SlotInfo slotInfo, string cacheKey, out array<ResourceName> outValidPrefabs)
	{
		int items;
		
		string loadoutSlotAreaTypeStr = PQD_Helpers.GetLoadoutSlotAreaTypeString(slotInfo.slot);
		cacheKey = string.Format("%1_%2", cacheKey, loadoutSlotAreaTypeStr);
		
		if (TryGetPrefabsFromCache(cacheKey, outValidPrefabs, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}

		array<ResourceName> validPrefabs = {};
		
		BaseLoadoutClothComponent clothComponent;
		LoadoutAreaType itemAreaType;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();

		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (arsenalItem.GetItemMode() != SCR_EArsenalItemMode.DEFAULT)
				continue;

			itemEntity = GetGame().SpawnEntityPrefabLocal(arsenalItem.GetItemResource(), world, null);
			if (!itemEntity)
				continue;
			
			clothComponent = BaseLoadoutClothComponent.Cast(itemEntity.FindComponent(BaseLoadoutClothComponent));
			if (clothComponent)
			{
				itemAreaType = clothComponent.GetAreaType();
				
				if (itemAreaType)
				{
					if (loadoutSlotAreaTypeStr == itemAreaType.Type().ToString())
					{
						outValidPrefabs.Insert(arsenalItem.GetItemResourceName());
						validPrefabs.Insert(arsenalItem.GetItemResourceName());
						items += 1;
					}
				}
			}
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
		
		m_SlotOptions.Set(cacheKey, validPrefabs);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}

	//------------------------------------------------------------------------------------------------
	int GetPrefabsForAttachmentSlots(PQD_SlotInfo slotInfo, string cacheKey, out array<ResourceName> outValidPrefabs)
	{
		int items;
		
		AttachmentSlotComponent slotComponent = AttachmentSlotComponent.Cast(slotInfo.slot.GetParentContainer());
		cacheKey = string.Format("%1_%2", cacheKey, slotComponent.GetAttachmentSlotType().Type().ToString());
		
		if (TryGetPrefabsFromCache(cacheKey, outValidPrefabs, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}
		
		array<ResourceName> validPrefabs = {};

		InventoryItemComponent itemComponent;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (arsenalItem.GetItemMode() != SCR_EArsenalItemMode.ATTACHMENT || arsenalItem.GetItemType() != SCR_EArsenalItemType.WEAPON_ATTACHMENT)
				continue;

			itemEntity = GetGame().SpawnEntityPrefabLocal(arsenalItem.GetItemResource(), world, null);

			itemComponent = InventoryItemComponent.Cast(itemEntity.FindComponent(InventoryItemComponent));
			if (!itemComponent)
			{
				Print(string.Format("[PQD] Prefab %1 has no InventoryItemComponent", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}

			if (!slotComponent.CanSetAttachment(itemEntity))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			outValidPrefabs.Insert(arsenalItem.GetItemResourceName());
			validPrefabs.Insert(arsenalItem.GetItemResourceName());
			items += 1;
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
		
		m_SlotOptions.Set(cacheKey, validPrefabs);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetPrefabsForMagazineWell(BaseMuzzleComponent muzzle, string cacheKey, out array<ResourceName> outValidPrefabs)
	{
		int items;
		
		string magazineWellString = muzzle.GetMagazineWell().Type().ToString();
		cacheKey = string.Format("%1_%2", cacheKey, magazineWellString);
		
		if (TryGetPrefabsFromCache(cacheKey, outValidPrefabs, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}
		
		array<ResourceName> validPrefabs = {};
		
		InventoryItemComponent itemComponent;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (arsenalItem.GetItemMode() != SCR_EArsenalItemMode.AMMUNITION)
				continue;
			
			itemEntity = GetGame().SpawnEntityPrefabLocal(arsenalItem.GetItemResource(), world, null);
			
			itemComponent = InventoryItemComponent.Cast(itemEntity.FindComponent(InventoryItemComponent));
			if (!itemComponent)
			{
				Print(string.Format("[PQD] Prefab %1 has no InventoryItemComponent", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			MagazineComponent mag = MagazineComponent.Cast(itemEntity.FindComponent(MagazineComponent));
			if (!mag)
			{
				Print(string.Format("[PQD] Prefab %1 has no MagazineComponent", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			BaseMagazineWell magWell = mag.GetMagazineWell();
			if (!magWell)
			{
				Print(string.Format("[PQD] Prefab %1 has no Magazine Well", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			SCR_ItemAttributeCollection attributes = SCR_ItemAttributeCollection.Cast(itemComponent.GetAttributes());
			if (!attributes || !attributes.IsVisible(itemComponent))
			{
				if (SCR_ArsenalComponent.Cast(itemEntity.FindComponent(SCR_ArsenalComponent)))
				{
					SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
					continue;
				}
			}
			
			if (magWell.Type().ToString() == magazineWellString)
			{
				outValidPrefabs.Insert(arsenalItem.GetItemResourceName());
				validPrefabs.Insert(arsenalItem.GetItemResourceName());
				items += 1;
			}
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
		
		m_SlotOptions.Set(cacheKey, validPrefabs);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetPrefabsForCharacterEquipmentSlot(EquipmentStorageSlot slotComponent, string cacheKey, out array<ResourceName> outValidPrefabs)
	{
		int items;
		
		cacheKey = string.Format("%1_%2", cacheKey, slotComponent.GetSourceName());
		
		if (TryGetPrefabsFromCache(cacheKey, outValidPrefabs, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}

		array<ResourceName> validPrefabs = {};

		InventoryItemComponent itemComponent;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (arsenalItem.GetItemMode() != SCR_EArsenalItemMode.DEFAULT || arsenalItem.GetItemType() != SCR_EArsenalItemType.EQUIPMENT)
				continue;

			itemEntity = GetGame().SpawnEntityPrefabLocal(arsenalItem.GetItemResource(), world, null);
			if (!itemEntity)
			{
				Print(string.Format("[PQD] Failed to spawn prefab: %1", arsenalItem.GetItemResource()), LogLevel.WARNING);
				continue;
			}

			itemComponent = InventoryItemComponent.Cast(itemEntity.FindComponent(InventoryItemComponent));
			if (!itemComponent)
			{
				Print(string.Format("[PQD] Prefab %1 has no InventoryItemComponent", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}

			if (!slotComponent.CanAttachItem(itemEntity))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			outValidPrefabs.Insert(arsenalItem.GetItemResourceName());
			validPrefabs.Insert(arsenalItem.GetItemResourceName());
			items += 1;
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
		
		m_SlotOptions.Set(cacheKey, validPrefabs);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetPrefabsForCharacterWeaponSlot(BaseWeaponComponent weapon, string cacheKey, out array<ResourceName> outValidPrefabs)
	{
		int items;
		
		string weaponSlotType = PQD_Helpers.GetWeaponTypeStringFromWeaponSlot(weapon);
		cacheKey = string.Format("%1_%2", cacheKey, weaponSlotType);
		
		if (TryGetPrefabsFromCache(cacheKey, outValidPrefabs, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}
		
		SCR_EArsenalItemMode modes;
		SCR_EArsenalItemType types;
		
		if (!PQD_Helpers.GetArsenalItemTypesAndModesForWeaponSlot(weaponSlotType, modes, types))
		{
			Print(string.Format("[PQD] Unknown weapon slot type %1", weaponSlotType), LogLevel.WARNING);
			return 0;
		}
		
		array<ResourceName> validPrefabs = {};
		
		InventoryItemComponent itemComponent;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (!SCR_Enum.HasPartialFlag(arsenalItem.GetItemMode(), modes) || !SCR_Enum.HasPartialFlag(arsenalItem.GetItemType(), types))
				continue;

			itemEntity = GetGame().SpawnEntityPrefabLocal(arsenalItem.GetItemResource(), world, null);

			itemComponent = InventoryItemComponent.Cast(itemEntity.FindComponent(InventoryItemComponent));
			if (!itemComponent)
			{
				Print(string.Format("[PQD] Prefab %1 has no InventoryItemComponent", arsenalItem.GetItemResourceName()), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			outValidPrefabs.Insert(arsenalItem.GetItemResourceName());
			validPrefabs.Insert(arsenalItem.GetItemResourceName());
			items += 1;
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
		
		m_SlotOptions.Set(cacheKey, validPrefabs);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateSubArsenalItems(ResourceName prefabResource, SCR_ArsenalComponent arsenalComponent)
	{
		if (m_ArsenalItemsKnownSubArsenals.Contains(prefabResource))
			return;
		
		array<SCR_ArsenalItem> subArsenalItems = {};
		arsenalComponent.GetFilteredArsenalItems(subArsenalItems);
		
		if (subArsenalItems.Count() < 1)
			return;
		
		m_ArsenalItemsKnownSubArsenals.Insert(prefabResource);
		
		foreach (SCR_ArsenalItem item : subArsenalItems)
		{
			m_ArsenalItemsKnownSubItems.Insert(item.GetItemResourceName());
		}
		
		Print(string.Format("[PQD] Added %1 items from sub-arsenal %2", subArsenalItems.Count(), prefabResource), LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsItemInAnySubArsenal(ResourceName prefabResource)
	{
		return m_ArsenalItemsKnownSubItems.Contains(prefabResource);
	}
	
	//------------------------------------------------------------------------------------------------
	int GetArsenalItemsByMode(SCR_EArsenalItemMode itemMode, SCR_EArsenalItemType itemType, out array<SCR_ArsenalItem> outItems)
	{
		int items;
		outItems.Clear();
		
		string cacheKey = string.Format("ARSENAL_ITEMS_%1_%2", 
			SCR_Enum.FlagsToString(SCR_EArsenalItemMode, itemMode, "_", ""), 
			SCR_Enum.FlagsToString(SCR_EArsenalItemType, itemType, "_", ""));
		
		if (TryGetArsenalItemsFromCache(cacheKey, outItems, items))
		{
			Print(string.Format("[PQD] Cache hit: %1 items for %2", items, cacheKey), LogLevel.DEBUG);
			return items;
		}
		
		array<SCR_ArsenalItem> validItems = {};
		
		foreach (SCR_ArsenalItem arsenalItem : m_ArsenalItems)
		{
			if (!SCR_Enum.HasPartialFlag(arsenalItem.GetItemMode(), itemMode))
				continue;
			
			if (!SCR_Enum.HasPartialFlag(arsenalItem.GetItemType(), itemType))
				continue;

			outItems.Insert(arsenalItem);
			validItems.Insert(arsenalItem);
			items += 1;
		}
		
		m_ArsenalItemTypes.Set(cacheKey, validItems);
		Print(string.Format("[PQD] Cached %1 items for %2", items, cacheKey), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetChoicesForSlotType(PQD_SlotInfo slotInfo, out array<ResourceName> outValidPrefabs)
	{
		int items;
		string slotType = slotInfo.slot.Type().ToString();
		string cacheKey = string.Format("%1_%2", SCR_Enum.GetEnumName(PQD_SlotType, slotInfo.slotType), slotType);
		
		switch (slotInfo.slotType)
		{
			case PQD_SlotType.CHARACTER_LOADOUT:
				items = GetPrefabsForLoadoutAreaTypeSlot(slotInfo, cacheKey, outValidPrefabs);
				break;
			case PQD_SlotType.ATTACHMENT:
				items = GetPrefabsForAttachmentSlots(slotInfo, cacheKey, outValidPrefabs);
				break;
			case PQD_SlotType.CHARACTER_WEAPON:
				items = GetPrefabsForCharacterWeaponSlot(BaseWeaponComponent.Cast(slotInfo.slot.GetParentContainer()), cacheKey, outValidPrefabs);
				break;
			case PQD_SlotType.CHARACTER_EQUIPMENT:
				items = GetPrefabsForCharacterEquipmentSlot(EquipmentStorageSlot.Cast(slotInfo.slot), cacheKey, outValidPrefabs);
				break;
			case PQD_SlotType.MAGAZINE:
				items = GetPrefabsForMagazineWell(BaseMuzzleComponent.Cast(slotInfo.slot.GetParentContainer()), cacheKey, outValidPrefabs);
				break;
		}
		
		Print(string.Format("[PQD] GetChoicesForSlotType: %1 items", items), LogLevel.DEBUG);
		return items;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetVisualIdentityChoicesForFaction(PQD_SlotIdentity slotInfo, out array<ref VisualIdentity> outVisualIdentities)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		SCR_Faction faction = SCR_Faction.Cast(factionManager.GetFactionByKey(slotInfo.factionKey));
		
		if (!faction)
		{
			Print(string.Format("[PQD] Invalid faction for key %1", slotInfo.factionKey), LogLevel.ERROR);
			return 0;
		}
		
		FactionIdentity factionIdentity = faction.GetFactionIdentity();
		if (!factionIdentity)
		{
			Print(string.Format("[PQD] Invalid faction identity for key %1", slotInfo.factionKey), LogLevel.ERROR);
			return 0;
		}
		
		factionIdentity.GetVisualIdentities(outVisualIdentities);
		
		Print(string.Format("[PQD] GetVisualIdentityChoicesForFaction: %1 items", outVisualIdentities.Count()), LogLevel.DEBUG);
		return outVisualIdentities.Count();
	}
}
