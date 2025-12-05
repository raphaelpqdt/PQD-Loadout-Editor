// PQD Loadout Editor - UI Components
// Author: PQD Team
// Version: 1.0.0

// Note: PQD_MessageType enum is defined in PQD_LoadoutEditor_Enums.c

//------------------------------------------------------------------------------------------------
//! Category button click handler
class PQD_CategoryButtonHandler : ScriptedWidgetEventHandler
{
	protected PQD_CategoryButtonsUIComponent m_pParent;
	protected int m_iIndex;
	
	void Init(PQD_CategoryButtonsUIComponent parent, int index)
	{
		m_pParent = parent;
		m_iIndex = index;
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;
		
		if (m_pParent)
			m_pParent.OnCategoryButtonClicked(m_iIndex);
		
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Status message UI component
class PQD_StatusMessageUIComponent : ScriptedWidgetComponent
{
	protected TextWidget m_wMessageText;
	protected ImageWidget m_wBackground;
	protected Widget m_wRoot;
	protected Widget m_wStatusMessage;
	
	protected float m_fDisplayTimer = 0;
	protected float m_fDisplayDuration = 3.0;
	protected bool m_bIsShowing = false;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		
		// Try to find StatusMessage first, then look for children directly
		m_wStatusMessage = w.FindAnyWidget("StatusMessage");
		if (!m_wStatusMessage)
			m_wStatusMessage = w.FindAnyWidget("StatusOverlay");
		
		// Look for text widget
		m_wMessageText = TextWidget.Cast(w.FindAnyWidget("StatusText"));
		if (!m_wMessageText)
			m_wMessageText = TextWidget.Cast(w.FindAnyWidget("MessageText"));
		
		// Look for background widget
		m_wBackground = ImageWidget.Cast(w.FindAnyWidget("StatusBackground"));
		if (!m_wBackground)
			m_wBackground = ImageWidget.Cast(w.FindAnyWidget("Background"));
		
		// Hide by default
		if (m_wStatusMessage)
			m_wStatusMessage.SetVisible(false);
		
		Print(string.Format("[PQD] StatusMessage component attached. Root=%1, StatusMessage=%2, Text=%3, BG=%4", 
			m_wRoot, m_wStatusMessage, m_wMessageText, m_wBackground), LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowMessage(string message, PQD_MessageType type = PQD_MessageType.OK)
	{
		if (!m_wMessageText)
		{
			Print(string.Format("[PQD] StatusMessage: Cannot show message, text widget is null. Message: %1", message), LogLevel.WARNING);
			return;
		}
		
		m_wMessageText.SetText(message);
		
		Color bgColor;
		switch (type)
		{
			case PQD_MessageType.OK:
				bgColor = new Color(0.1, 0.6, 0.2, 0.9);
				break;
			case PQD_MessageType.WARNING:
				bgColor = new Color(0.8, 0.6, 0.1, 0.9);
				break;
			case PQD_MessageType.ERROR:
				bgColor = new Color(0.7, 0.2, 0.1, 0.9);
				break;
		}
		
		if (m_wBackground)
			m_wBackground.SetColor(bgColor);
		
		if (m_wStatusMessage)
			m_wStatusMessage.SetVisible(true);
		
		m_bIsShowing = true;
		m_fDisplayTimer = 0;
		
		Print(string.Format("[PQD] StatusMessage: %1 (type=%2)", message, type), LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	void HideMessage()
	{
		if (m_wStatusMessage)
			m_wStatusMessage.SetVisible(false);
		m_bIsShowing = false;
	}
	
	//------------------------------------------------------------------------------------------------
	void Update(float tDelta)
	{
		if (!m_bIsShowing)
			return;
		
		m_fDisplayTimer += tDelta;
		if (m_fDisplayTimer >= m_fDisplayDuration)
			HideMessage();
	}
}

//------------------------------------------------------------------------------------------------
//! Slots list UI component
class PQD_SlotsUIComponent : ScriptedWidgetComponent
{
	ref ScriptInvoker m_OnItemClicked = new ScriptInvoker();
	ref ScriptInvoker m_OnItemFocused = new ScriptInvoker();
	ref ScriptInvoker m_OnPanelFocused = new ScriptInvoker();
	
	protected Widget m_wRoot;
	protected ScrollLayoutWidget m_wScrollLayout;
	protected Widget m_wContent;
	
	protected ref array<Widget> m_aItems = {};
	protected ref array<ref Managed> m_aItemData = {};
	protected int m_iFocusedItem = -1;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		
		// Try multiple widget names for compatibility
		m_wScrollLayout = ScrollLayoutWidget.Cast(w.FindAnyWidget("ScrollLayout0"));
		if (!m_wScrollLayout)
			m_wScrollLayout = ScrollLayoutWidget.Cast(w.FindAnyWidget("ScrollLayout"));
		
		m_wContent = w.FindAnyWidget("List");
		if (!m_wContent)
			m_wContent = w.FindAnyWidget("Content");
		
		// If still not found, use the first child of scroll layout
		if (!m_wContent && m_wScrollLayout)
		{
			m_wContent = m_wScrollLayout.GetChildren();
		}
		
		// If still not found, use the widget itself (for simple layouts like ItemList in inventory panel)
		if (!m_wContent)
		{
			m_wContent = w;
			Print(string.Format("[PQD] SlotsUIComponent: Using widget itself as content: %1", w.GetName()), LogLevel.DEBUG);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_wContent)
		{
			Widget child = m_wContent.GetChildren();
			while (child)
			{
				Widget next = child.GetSibling();
				child.RemoveFromHierarchy();
				child = next;
			}
		}
		
		m_aItems.Clear();
		m_aItemData.Clear();
		m_iFocusedItem = -1;
	}
	
	//------------------------------------------------------------------------------------------------
	int AddItem_Slot(PQD_SlotInfo slotInfo)
	{
		if (!m_wContent)
			return -1;
		
		Widget itemWidget = GetGame().GetWorkspace().CreateWidgets("{A8DB68751DBB2CAD}UI/PQDLoadoutEditor/PQD_SlotItem.layout", m_wContent);
		if (!itemWidget)
			return -1;
		
		int index = m_aItems.Count();
		itemWidget.SetZOrder(index);
		
		// Set up widget content - try different widget names for compatibility
		TextWidget nameText = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
		if (nameText)
		{
			string displayText;
			// If we have an item name, show it; otherwise show just the slot name or "Empty" for truly empty slots
			if (!slotInfo.itemName.IsEmpty())
				displayText = string.Format("%1: %2", slotInfo.slotName, slotInfo.itemName);
			else if (slotInfo.prefab && !slotInfo.prefab.IsEmpty())
				displayText = slotInfo.slotName; // Has a prefab but no item name, just show slot name
			else if (slotInfo.slotType == PQD_SlotType.MESSAGE)
				displayText = slotInfo.slotName; // For headers/messages, don't add ": Empty"
			else
				displayText = string.Format("%1: Empty", slotInfo.slotName);
			nameText.SetText(displayText);
		}
		
		// Show storage indicator if the slot has editable storage
		ImageWidget storageIndicator = ImageWidget.Cast(itemWidget.FindAnyWidget("ImageHasStorage"));
		if (storageIndicator)
			storageIndicator.SetVisible(slotInfo.hasStorage);
		
		// Setup mini preview 3D if we have a prefab
		if (slotInfo.prefab && !slotInfo.prefab.IsEmpty())
		{
			SetupItemPreview(itemWidget, slotInfo.prefab);
		}
		
		// Add button handler - the root widget IS the button
		ButtonWidget button = ButtonWidget.Cast(itemWidget);
		if (button)
		{
			PQD_SlotButtonHandler handler = new PQD_SlotButtonHandler();
			handler.Init(this, index);
			button.AddHandler(handler);
		}
		
		m_aItems.Insert(itemWidget);
		m_aItemData.Insert(slotInfo);
		
		return index;
	}
	
	//------------------------------------------------------------------------------------------------
	int AddItem_Loadout(PQD_SlotLoadout loadoutInfo)
	{
		if (!m_wContent)
			return -1;

		Widget itemWidget = GetGame().GetWorkspace().CreateWidgets("{A8DB68751DBB2CAD}UI/PQDLoadoutEditor/PQD_SlotItem.layout", m_wContent);
		if (!itemWidget)
			return -1;

		int index = m_aItems.Count();
		itemWidget.SetZOrder(index);

		TextWidget nameText = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
		if (nameText)
			nameText.SetText(string.Format("Slot %1: %2", loadoutInfo.loadoutSlotId + 1, loadoutInfo.metadataClothes));

		// Setup 3D character preview with equipped items if loadout has data
		if (loadoutInfo.prefab && !loadoutInfo.prefab.IsEmpty() && !loadoutInfo.data.IsEmpty())
		{
			Print(string.Format("[PQD] AddItem_Loadout: Setting up equipped preview for slot %1", loadoutInfo.loadoutSlotId), LogLevel.NORMAL);
			SetupLoadoutPreviewWithEquipment(itemWidget, loadoutInfo.prefab, loadoutInfo.data);
		}
		else if (loadoutInfo.prefab && !loadoutInfo.prefab.IsEmpty())
		{
			Print(string.Format("[PQD] AddItem_Loadout: Setting up base preview for slot %1", loadoutInfo.loadoutSlotId), LogLevel.NORMAL);
			SetupItemPreview(itemWidget, loadoutInfo.prefab);
		}
		else
		{
			Print(string.Format("[PQD] AddItem_Loadout: Slot %1 has no prefab (empty slot)", loadoutInfo.loadoutSlotId), LogLevel.DEBUG);
		}

		// The root widget IS the button
		ButtonWidget button = ButtonWidget.Cast(itemWidget);
		if (button)
		{
			PQD_SlotButtonHandler handler = new PQD_SlotButtonHandler();
			handler.Init(this, index);
			button.AddHandler(handler);
		}

		m_aItems.Insert(itemWidget);
		m_aItemData.Insert(loadoutInfo);

		return index;
	}
	
	//------------------------------------------------------------------------------------------------
	int AddItem_Choice(PQD_SlotChoice choiceData)
	{
		if (!m_wContent)
			return -1;
		
		Widget itemWidget = GetGame().GetWorkspace().CreateWidgets("{A8DB68751DBB2CAD}UI/PQDLoadoutEditor/PQD_SlotItem.layout", m_wContent);
		if (!itemWidget)
			return -1;
		
		int index = m_aItems.Count();
		itemWidget.SetZOrder(index);
		
		TextWidget nameText = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
		if (nameText)
		{
			string displayText = choiceData.slotName;
			if (!choiceData.itemName.IsEmpty())
				displayText = string.Format("%1 %2", choiceData.slotName, choiceData.itemName);
			nameText.SetText(displayText);
		}
		
		// Setup mini preview 3D if we have a prefab
		if (choiceData.prefab && !choiceData.prefab.IsEmpty())
		{
			SetupItemPreview(itemWidget, choiceData.prefab);
			
			// Show rank and supply cost for arsenal items
			SetSupplyAndRankInformation(itemWidget, choiceData.prefab);
		}
		
		// Visual feedback for disabled items
		if (!choiceData.slotEnabled)
		{
			Widget blockedPanel = itemWidget.FindAnyWidget("BlockedPanel");
			if (blockedPanel)
				blockedPanel.SetVisible(true);
			
			TextWidget txt = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
			if (txt)
				txt.SetColor(new Color(0.5, 0.5, 0.5, 0.8));
		}
		
		// The root widget IS the button
		ButtonWidget button = ButtonWidget.Cast(itemWidget);
		if (button)
		{
			PQD_SlotButtonHandler handler = new PQD_SlotButtonHandler();
			handler.Init(this, index);
			button.AddHandler(handler);
		}
		
		m_aItems.Insert(itemWidget);
		m_aItemData.Insert(choiceData);
		
		return index;
	}
	
	//------------------------------------------------------------------------------------------------
	int AddItem_Option(PQD_EditorOptionData optionData)
	{
		if (!m_wContent)
			return -1;
		
		Widget itemWidget = GetGame().GetWorkspace().CreateWidgets("{A8DB68751DBB2CAD}UI/PQDLoadoutEditor/PQD_SlotItem.layout", m_wContent);
		if (!itemWidget)
			return -1;
		
		int index = m_aItems.Count();
		itemWidget.SetZOrder(index);
		
		TextWidget nameText = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
		if (nameText)
			nameText.SetText(optionData.optionLabel);
		
		// Setup mini preview 3D if we have a prefab
		if (optionData.optionPrefab && !optionData.optionPrefab.IsEmpty())
		{
			SetupItemPreview(itemWidget, optionData.optionPrefab);
		}
		
		// Show blocked overlay if not available
		if (!optionData.optionEnabled)
		{
			Widget blockedPanel = itemWidget.FindAnyWidget("BlockedPanel");
			if (blockedPanel)
				blockedPanel.SetVisible(true);
			
			TextWidget txt = TextWidget.Cast(itemWidget.FindAnyWidget("SlotText"));
			if (txt)
				txt.SetColor(new Color(0.5, 0.5, 0.5, 0.8));
		}
		
		// The root widget IS the button
		ButtonWidget button = ButtonWidget.Cast(itemWidget);
		if (button)
		{
			PQD_SlotButtonHandler handler = new PQD_SlotButtonHandler();
			handler.Init(this, index);
			button.AddHandler(handler);
		}
		
		m_aItems.Insert(itemWidget);
		m_aItemData.Insert(optionData);
		
		return index;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SetupItemPreview(Widget itemWidget, ResourceName prefab)
	{
		if (!prefab || prefab.IsEmpty())
			return;
		
		ItemPreviewWidget previewWidget = ItemPreviewWidget.Cast(itemWidget.FindAnyWidget("SlotPreview"));
		if (!previewWidget)
			return;
		
		// Get the preview manager
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;
		
		ItemPreviewManagerEntity previewManager = world.GetItemPreviewManager();
		if (!previewManager)
			return;
		
		// Load the prefab resource
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return;
		
		// Set the preview - create a temporary entity for preview
		previewWidget.SetVisible(true);
		previewManager.SetPreviewItemFromPrefab(previewWidget, prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Setup loadout preview with equipped items from serialized data
	protected void SetupLoadoutPreviewWithEquipment(Widget itemWidget, ResourceName prefab, string loadoutData)
	{
		if (!prefab || prefab.IsEmpty() || loadoutData.IsEmpty())
		{
			SetupItemPreview(itemWidget, prefab);
			return;
		}

		ItemPreviewWidget previewWidget = ItemPreviewWidget.Cast(itemWidget.FindAnyWidget("SlotPreview"));
		if (!previewWidget)
			return;

		// Get the preview manager
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		ItemPreviewManagerEntity previewManager = world.GetItemPreviewManager();
		if (!previewManager)
			return;

		// Load the base character prefab
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return;

		// Create a NEW preview entity for this loadout (don't reuse existing ones)
		// We need a unique entity per loadout to show different equipment
		IEntity previewEntity = GetGame().SpawnEntityPrefabLocal(resource, world);
		if (!previewEntity)
		{
			Print("[PQD] SetupLoadoutPreviewWithEquipment: Failed to spawn preview entity", LogLevel.WARNING);
			SetupItemPreview(itemWidget, prefab);
			return;
		}

		// Parse the loadout data
		SCR_PlayerLoadoutData playerLoadoutData = ConvertJsonToLoadoutData(loadoutData);
		if (!playerLoadoutData || (playerLoadoutData.Clothings.IsEmpty() && playerLoadoutData.Weapons.IsEmpty()))
		{
			Print("[PQD] SetupLoadoutPreviewWithEquipment: No equipment data found, using base preview", LogLevel.DEBUG);
			previewWidget.SetVisible(true);
			previewManager.SetPreviewItem(previewWidget, previewEntity, null, true);
			return;
		}

		Print(string.Format("[PQD] SetupLoadoutPreviewWithEquipment: Applying %1 clothes, %2 weapons",
			playerLoadoutData.Clothings.Count(), playerLoadoutData.Weapons.Count()), LogLevel.NORMAL);

		// Delete existing children to avoid conflicts
		DeleteChildrens(previewEntity, false);

		// Apply clothing
		EquipedLoadoutStorageComponent loadoutStorage = EquipedLoadoutStorageComponent.Cast(previewEntity.FindComponent(EquipedLoadoutStorageComponent));
		if (loadoutStorage)
		{
			foreach (SCR_ClothingLoadoutData clothingData : playerLoadoutData.Clothings)
			{
				InventoryStorageSlot slot = loadoutStorage.GetSlot(clothingData.SlotIdx);
				if (!slot)
					continue;

				Resource clothResource = Resource.Load(clothingData.ClothingPrefab);
				if (!clothResource || !clothResource.IsValid())
					continue;

				IEntity cloth = GetGame().SpawnEntityPrefabLocal(clothResource, previewEntity.GetWorld());
				if (cloth)
					slot.AttachEntity(cloth);
			}
		}

		// Apply weapons
		EquipedWeaponStorageComponent weaponStorage = EquipedWeaponStorageComponent.Cast(previewEntity.FindComponent(EquipedWeaponStorageComponent));
		if (weaponStorage)
		{
			foreach (SCR_WeaponLoadoutData weaponData : playerLoadoutData.Weapons)
			{
				InventoryStorageSlot slot = weaponStorage.GetSlot(weaponData.SlotIdx);
				if (!slot)
					continue;

				Resource weaponResource = Resource.Load(weaponData.WeaponPrefab);
				if (!weaponResource || !weaponResource.IsValid())
					continue;

				IEntity weapon = GetGame().SpawnEntityPrefabLocal(weaponResource, previewEntity.GetWorld());
				if (weapon)
					slot.AttachEntity(weapon);
			}
		}

		// Select default weapon for proper pose
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(previewEntity.FindComponent(BaseWeaponManagerComponent));
		if (weaponManager)
		{
			int weaponDefaultIndex = weaponManager.GetDefaultWeaponIndex();
			if (weaponDefaultIndex > -1)
			{
				array<WeaponSlotComponent> outSlots = {};
				weaponManager.GetWeaponsSlots(outSlots);
				foreach (WeaponSlotComponent weaponSlot : outSlots)
				{
					if (weaponSlot.GetWeaponSlotIndex() == weaponDefaultIndex)
					{
						weaponManager.SelectWeapon(weaponSlot);
						break;
					}
				}
			}
		}

		// Set the preview
		previewWidget.SetVisible(true);
		previewManager.SetPreviewItem(previewWidget, previewEntity, null, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Helper: Delete all children from an entity
	protected void DeleteChildrens(IEntity ent, bool immediate)
	{
		IEntity child = ent.GetChildren();
		while (child)
		{
			IEntity sibling = child.GetSibling();
			SCR_EntityHelper.DeleteEntityAndChildren(child);
			child = sibling;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Helper: Convert JSON loadout data to SCR_PlayerLoadoutData structure (adapted from PQD_ModdedDeployMenu)
	protected SCR_PlayerLoadoutData ConvertJsonToLoadoutData(string loadoutData)
	{
		if (loadoutData.IsEmpty())
			return null;

		SCR_PlayerLoadoutData result = new SCR_PlayerLoadoutData();

		// Parse the JSON
		SCR_JsonLoadContext context = new SCR_JsonLoadContext();
		if (!context.ImportFromString(loadoutData))
		{
			Print("[PQD] ConvertJsonToLoadoutData: Failed to parse JSON", LogLevel.WARNING);
			return null;
		}

		// Start reading the arsenalLoadout object
		if (!context.StartObject(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_KEY))
		{
			Print("[PQD] ConvertJsonToLoadoutData: No arsenalLoadout object", LogLevel.WARNING);
			return null;
		}

		// Read storages array
		int storageCount;
		if (!context.StartArray("storages", storageCount))
		{
			context.EndObject();
			return result;
		}

		// Process each storage
		for (int nStorage = 0; nStorage < storageCount; nStorage++)
		{
			if (!context.StartObject())
				continue;

			// Read storage ID to determine type
			string storageId;
			if (!context.ReadValue("id", storageId))
			{
				context.EndObject();
				continue;
			}

			// Determine storage type based on ID
			bool isWeaponStorage = storageId.Contains("EquipedWeaponStorageComponent");
			bool isClothingStorage = storageId.Contains("SCR_CharacterInventoryStorageComponent");

			// Read slots
			int slotCount;
			if (!context.StartMap("slots", slotCount))
			{
				context.EndObject();
				continue;
			}

			// Process each slot
			for (int i = 0; i < slotCount; i++)
			{
				string slotIdxStr;
				if (!context.ReadMapKey(i, slotIdxStr))
					continue;

				int slotIdx = slotIdxStr.ToInt(-1);
				if (slotIdx == -1)
					continue;

				if (!context.StartObject(slotIdxStr))
					continue;

				// Read the prefab
				ResourceName itemPrefab;
				if (context.ReadValue("prefab", itemPrefab) && !itemPrefab.IsEmpty())
				{
					if (isWeaponStorage)
					{
						SCR_WeaponLoadoutData weaponData = new SCR_WeaponLoadoutData();
						weaponData.SlotIdx = slotIdx;
						weaponData.WeaponPrefab = itemPrefab;
						weaponData.Active = (slotIdx == 0);
						result.Weapons.Insert(weaponData);
					}
					else if (isClothingStorage)
					{
						SCR_ClothingLoadoutData clothingData = new SCR_ClothingLoadoutData();
						clothingData.SlotIdx = slotIdx;
						clothingData.ClothingPrefab = itemPrefab;
						result.Clothings.Insert(clothingData);
					}
				}

				context.EndObject();
			}

			context.EndMap();
			context.EndObject();
		}

		context.EndArray();
		context.EndObject();

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Set supply cost and rank requirement information on item widget
	protected void SetSupplyAndRankInformation(Widget itemWidget, ResourceName prefab)
	{
		if (!prefab || prefab.IsEmpty())
			return;
		
		PQD_Cache cache = PQD_Cache.GetInstance();
		if (!cache)
			return;
		
		// Get cost and rank from cache
		float cost;
		SCR_ECharacterRank requiredRank;
		cache.GetArsenalItemCostAndRank(prefab, cost, requiredRank);
		
		// Show/hide info layout
		Widget infoLayout = itemWidget.FindAnyWidget("InfoLayout");
		if (!infoLayout)
			return;
		
		bool hasInfoToShow = false;
		
		// Set supply cost
		Widget supplyOverlay = itemWidget.FindAnyWidget("SupplyOverlay");
		if (supplyOverlay)
		{
			if (cost > 0 && cache.AreSuppliesEnabled())
			{
				TextWidget supplyText = TextWidget.Cast(itemWidget.FindAnyWidget("SupplyText"));
				if (supplyText)
					supplyText.SetText(Math.Round(cost).ToString());
				supplyOverlay.SetVisible(true);
				hasInfoToShow = true;
			}
			else
			{
				supplyOverlay.SetVisible(false);
			}
		}
		
		// Set required rank
		Widget rankOverlay = itemWidget.FindAnyWidget("RankOverlay");
		if (rankOverlay)
		{
			if (requiredRank != SCR_ECharacterRank.INVALID && requiredRank != SCR_ECharacterRank.RENEGADE)
			{
				TextWidget rankText = TextWidget.Cast(itemWidget.FindAnyWidget("RankText"));
				if (rankText)
					rankText.SetText(typename.EnumToString(SCR_ECharacterRank, requiredRank));
				rankOverlay.SetVisible(true);
				hasInfoToShow = true;
			}
			else
			{
				rankOverlay.SetVisible(false);
			}
		}
		
		infoLayout.SetVisible(hasInfoToShow);
	}
	
	//------------------------------------------------------------------------------------------------
	Widget GetItem(int index)
	{
		if (index < 0 || index >= m_aItems.Count())
			return null;
		return m_aItems[index];
	}
	
	//------------------------------------------------------------------------------------------------
	Managed GetItemData(int index)
	{
		if (index < 0 || index >= m_aItemData.Count())
			return null;
		return m_aItemData[index];
	}
	
	//------------------------------------------------------------------------------------------------
	int GetFocusedItem()
	{
		return m_iFocusedItem;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetItemCount()
	{
		return m_aItems.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	void SetFocusedItem(int index)
	{
		SetFocus(index);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetFocus(int index)
	{
		if (index < 0 || index >= m_aItems.Count())
			return;
		
		Widget item = m_aItems[index];
		if (!item)
			return;
		
		// The item itself is the button widget
		ButtonWidget button = ButtonWidget.Cast(item);
		if (button)
			GetGame().GetWorkspace().SetFocusedWidget(button);
		
		m_iFocusedItem = index;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetFocusedItemSelected()
	{
		if (m_iFocusedItem >= 0 && m_iFocusedItem < m_aItemData.Count())
		{
			m_OnItemClicked.Invoke(this, m_aItemData[m_iFocusedItem], 0);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnItemClick(int index, int button)
	{
		if (index < 0 || index >= m_aItemData.Count())
			return;
		
		m_iFocusedItem = index;
		m_OnItemClicked.Invoke(this, m_aItemData[index], button);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnItemFocus(int index)
	{
		if (index < 0 || index >= m_aItemData.Count())
			return;
		
		m_iFocusedItem = index;
		m_OnItemFocused.Invoke(this, m_aItemData[index]);
	}
}

//------------------------------------------------------------------------------------------------
//! Button handler for slot items
class PQD_SlotButtonHandler : ScriptedWidgetComponent
{
	protected PQD_SlotsUIComponent m_Parent;
	protected int m_iIndex;
	
	//------------------------------------------------------------------------------------------------
	void Init(PQD_SlotsUIComponent parent, int index)
	{
		m_Parent = parent;
		m_iIndex = index;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Parent)
			m_Parent.OnItemClick(m_iIndex, button);
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		if (m_Parent)
			m_Parent.OnItemFocus(m_iIndex);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Multifunction slot info display component
class PQD_MultifunctionSlotUIComponent : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected TextWidget m_wSlotName;
	protected TextWidget m_wItemName;
	protected ImageWidget m_wItemIcon;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		m_wSlotName = TextWidget.Cast(w.FindAnyWidget("SlotName"));
		m_wItemName = TextWidget.Cast(w.FindAnyWidget("ItemName"));
		m_wItemIcon = ImageWidget.Cast(w.FindAnyWidget("ItemIcon"));
	}
	
	//------------------------------------------------------------------------------------------------
	void SetData(Managed data)
	{
		PQD_SlotInfo slotInfo = PQD_SlotInfo.Cast(data);
		if (slotInfo)
		{
			if (m_wSlotName)
				m_wSlotName.SetText(slotInfo.slotName);
			if (m_wItemName)
				m_wItemName.SetText(slotInfo.itemName);
			return;
		}
		
		PQD_EditorOptionData optionData = PQD_EditorOptionData.Cast(data);
		if (optionData)
		{
			if (m_wSlotName)
				m_wSlotName.SetText(optionData.editorOptionLabel);
			if (m_wItemName)
				m_wItemName.SetText(optionData.optionLabel);
			return;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_wSlotName)
			m_wSlotName.SetText("");
		if (m_wItemName)
			m_wItemName.SetText("");
	}
}

//------------------------------------------------------------------------------------------------
//! Category buttons UI component
class PQD_CategoryButtonsUIComponent : ScriptedWidgetComponent
{
	ref ScriptInvoker m_OnCategoryChangedInvoker = new ScriptInvoker();
	
	protected Widget m_wRoot;
	protected ref array<Widget> m_aButtonWidgets = {};
	protected ref array<ref PQD_CategoryButtonHandler> m_aButtonHandlers = {}; // Keep reference to prevent garbage collection
	protected int m_iCurrentCategoryIndex = 0;
	protected PQD_SlotCategory m_eCurrentCategory = PQD_SlotCategory.CHARACTER_CLOTHING;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		m_aButtonWidgets.Clear();
		m_aButtonHandlers.Clear();
		
		// Find and setup category buttons - look for ButtonWidgets by name
		array<string> buttonNames = {"ButtonClothing", "ButtonGear", "ButtonItems", "ButtonLoadouts", "ButtonSettings"};
		
		Print(string.Format("[PQD] CategoryButtonsUIComponent HandlerAttached - Root widget: %1", w), LogLevel.DEBUG);
		
		foreach (int i, string buttonName : buttonNames)
		{
			Widget buttonWidget = w.FindAnyWidget(buttonName);
			if (buttonWidget)
			{
				m_aButtonWidgets.Insert(buttonWidget);
				
				// Add click handler and keep reference
				PQD_CategoryButtonHandler handler = new PQD_CategoryButtonHandler();
				handler.Init(this, i);
				buttonWidget.AddHandler(handler);
				m_aButtonHandlers.Insert(handler);
				
				Print(string.Format("[PQD] Found category button: %1 (index %2)", buttonName, i), LogLevel.DEBUG);
			}
			else
			{
				Print(string.Format("[PQD] WARNING: Category button NOT found: %1", buttonName), LogLevel.WARNING);
			}
		}
		
		Print(string.Format("[PQD] Total category buttons found: %1", m_aButtonWidgets.Count()), LogLevel.DEBUG);
		
		// Initial visual update
		UpdateCategoryVisuals();
	}
	
	//------------------------------------------------------------------------------------------------
	void SelectPreviousCategory()
	{
		if (m_aButtonWidgets.Count() == 0)
		{
			Print("[PQD] SelectPreviousCategory: No buttons available!", LogLevel.WARNING);
			return;
		}
		
		int newIndex = m_iCurrentCategoryIndex - 1;
		
		// Wrap around
		if (newIndex < 0)
			newIndex = m_aButtonWidgets.Count() - 1;
		
		Print(string.Format("[PQD] SelectPreviousCategory: %1 -> %2", m_iCurrentCategoryIndex, newIndex), LogLevel.DEBUG);
		SetCategoryByIndex(newIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	void SelectNextCategory()
	{
		if (m_aButtonWidgets.Count() == 0)
		{
			Print("[PQD] SelectNextCategory: No buttons available!", LogLevel.WARNING);
			return;
		}
		
		int newIndex = m_iCurrentCategoryIndex + 1;
		
		// Wrap around
		if (newIndex >= m_aButtonWidgets.Count())
			newIndex = 0;
		
		Print(string.Format("[PQD] SelectNextCategory: %1 -> %2", m_iCurrentCategoryIndex, newIndex), LogLevel.DEBUG);
		SetCategoryByIndex(newIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCategoryByIndex(int index)
	{
		if (index < 0 || index >= 5)
		{
			Print(string.Format("[PQD] SetCategoryByIndex: Invalid index %1", index), LogLevel.WARNING);
			return;
		}
		
		m_iCurrentCategoryIndex = index;
		
		switch (index)
		{
			case 0: m_eCurrentCategory = PQD_SlotCategory.CHARACTER_CLOTHING; break;
			case 1: m_eCurrentCategory = PQD_SlotCategory.CHARACTER_GEAR; break;
			case 2: m_eCurrentCategory = PQD_SlotCategory.CHARACTER_ITEMS; break;
			case 3: m_eCurrentCategory = PQD_SlotCategory.LOADOUTS; break;
			case 4: m_eCurrentCategory = PQD_SlotCategory.SETTINGS; break;
		}
		
		Print(string.Format("[PQD] Category changed to index %1, category: %2", index, m_eCurrentCategory), LogLevel.DEBUG);
		
		UpdateCategoryVisuals();
		
		// Play sound feedback
		PQD_Helpers.PlaySound("SoundUI_Select");
		
		m_OnCategoryChangedInvoker.Invoke();
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateCategoryVisuals()
	{
		// Highlight active category button
		for (int i = 0; i < m_aButtonWidgets.Count(); i++)
		{
			Widget buttonWidget = m_aButtonWidgets[i];
			if (!buttonWidget)
				continue;
			
			// Find TabBG child to change color
			ImageWidget tabBG = ImageWidget.Cast(buttonWidget.FindAnyWidget("TabBG"));
			TextWidget label = TextWidget.Cast(buttonWidget.FindAnyWidget("Label"));
			
			// Set visual distinction for active vs inactive
			if (i == m_iCurrentCategoryIndex)
			{
				buttonWidget.SetOpacity(1.0);
				if (tabBG)
					tabBG.SetColor(new Color(0.761, 0.386, 0.078, 0.95)); // Bright orange for active
				if (label)
					label.SetColor(new Color(1.0, 1.0, 1.0, 1.0)); // White text
			}
			else
			{
				buttonWidget.SetOpacity(0.8);
				if (tabBG)
					tabBG.SetColor(new Color(0.12, 0.12, 0.14, 0.85)); // Dark grey for inactive
				if (label)
					label.SetColor(new Color(0.7, 0.7, 0.75, 0.9)); // Grey text
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnCategoryButtonClicked(int index)
	{
		Print(string.Format("[PQD] Category button clicked: %1", index), LogLevel.DEBUG);
		SetCategoryByIndex(index);
	}
	
	//------------------------------------------------------------------------------------------------
	PQD_SlotCategory GetCurrentCategoryCharacter()
	{
		return m_eCurrentCategory;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetCurrentCategoryIndex()
	{
		return m_iCurrentCategoryIndex;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ShouldViewCharacterStorageCategory(PQD_StorageType storageType)
	{
		switch (m_eCurrentCategory)
		{
			case PQD_SlotCategory.CHARACTER_CLOTHING:
				return storageType == PQD_StorageType.CHARACTER_LOADOUT;
			case PQD_SlotCategory.CHARACTER_GEAR:
				return storageType == PQD_StorageType.CHARACTER_EQUIPMENT ||
					   storageType == PQD_StorageType.CHARACTER_WEAPON;
			case PQD_SlotCategory.CHARACTER_ITEMS:
				return true;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetEnabled(bool enabled)
	{
		foreach (Widget buttonWidget : m_aButtonWidgets)
		{
			if (buttonWidget)
				buttonWidget.SetEnabled(enabled);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Inventory panel UI component - displays available arsenal items
class PQD_InventoryPanelUIComponent : ScriptedWidgetComponent
{
	ref ScriptInvoker m_OnItemClicked = new ScriptInvoker();
	ref ScriptInvoker m_OnItemFocused = new ScriptInvoker();

	protected Widget m_wRoot;
	protected PQD_SlotsUIComponent m_wItemList;
	protected bool m_bIsInSubMenu = false;
	protected PQD_ItemCategory m_eCurrentCategory;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		
		Widget listWidget = w.FindAnyWidget("ItemList");
		if (listWidget)
		{
			m_wItemList = PQD_SlotsUIComponent.Cast(listWidget.FindHandler(PQD_SlotsUIComponent));
			if (m_wItemList)
			{
				m_wItemList.m_OnItemClicked.Insert(OnItemClick);
				m_wItemList.m_OnItemFocused.Insert(OnItemFocus);
			}
		}
		
		if (m_wRoot)
			m_wRoot.SetVisible(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnItemClick(PQD_SlotsUIComponent comp, Managed data, int button)
	{
		m_OnItemClicked.Invoke(comp, data, button);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnItemFocus(PQD_SlotsUIComponent comp, Managed data)
	{
		m_OnItemFocused.Invoke(comp, data);
	}
	
	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_wItemList)
			m_wItemList.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	// Add a category header to the panel
	void AddCategoryHeader(string categoryName)
	{
		if (!m_wItemList)
		{
			Print("[PQD] AddCategoryHeader: m_wItemList is null!", LogLevel.WARNING);
			return;
		}

		// Create a disabled slot info to display the category header
		PQD_SlotInfo headerInfo = new PQD_SlotInfo();
		headerInfo.slotName = categoryName;
		headerInfo.itemName = "";
		headerInfo.slotEnabled = false;
		headerInfo.hasStorage = false;
		headerInfo.slotType = PQD_SlotType.MESSAGE;

		m_wItemList.AddItem_Slot(headerInfo);
	}

	//------------------------------------------------------------------------------------------------
	// Add a category choice to the panel
	void AddCategoryChoice(PQD_SlotChoiceCategory categoryChoice)
	{
		if (!m_wItemList)
		{
			Print("[PQD] AddCategoryChoice: m_wItemList is null!", LogLevel.WARNING);
			return;
		}

		if (!categoryChoice)
		{
			Print("[PQD] AddCategoryChoice: categoryChoice is null!", LogLevel.WARNING);
			return;
		}

		m_wItemList.AddItem_Choice(categoryChoice);
	}

	//------------------------------------------------------------------------------------------------
	// Add an arsenal item to the panel
	void AddArsenalItem(SCR_ArsenalItem arsenalItem)
	{
		if (!m_wItemList)
		{
			Print("[PQD] AddArsenalItem: m_wItemList is null!", LogLevel.WARNING);
			return;
		}

		if (!arsenalItem)
		{
			Print("[PQD] AddArsenalItem: arsenalItem is null!", LogLevel.WARNING);
			return;
		}

		ResourceName prefab = arsenalItem.GetItemResourceName();

		// Get item name from prefab - use PQD_Helpers to resolve name
		string itemName = PQD_Helpers.GetItemNameFromPrefab(prefab);

		PQD_SlotChoice choice = new PQD_SlotChoice();
		choice.prefab = prefab;
		choice.slotType = PQD_SlotType.ADD;
		choice.slotEnabled = true;
		choice.slotName = itemName;

		m_wItemList.AddItem_Choice(choice);
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowPanel()
	{
		if (m_wRoot)
			m_wRoot.SetVisible(true);
	}
	
	//------------------------------------------------------------------------------------------------
	void HidePanel()
	{
		if (m_wRoot)
			m_wRoot.SetVisible(false);
		m_bIsInSubMenu = false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsInSubMenu()
	{
		return m_bIsInSubMenu;
	}
	
	//------------------------------------------------------------------------------------------------
	void ExitSubMenu()
	{
		Print(string.Format("[PQD] ExitSubMenu called, was in category %1", m_eCurrentCategory), LogLevel.NORMAL);
		m_bIsInSubMenu = false;
		// Clear the category when exiting
		m_eCurrentCategory = PQD_ItemCategory.AMMUNITION; // Reset to default
		Print("[PQD] ExitSubMenu complete, IsInSubMenu now false", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void EnterCategory(PQD_ItemCategory category)
	{
		m_bIsInSubMenu = true;
		m_eCurrentCategory = category;
	}

	//------------------------------------------------------------------------------------------------
	PQD_ItemCategory GetCurrentCategory()
	{
		return m_eCurrentCategory;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wRoot;
	}
}

//------------------------------------------------------------------------------------------------
//! Rank and supply info component
class PQD_RankAndSupplyInfoComponent : ScriptedWidgetComponent
{
	static const float UPDATE_INTERVAL_SECONDS = 2;
	
	protected float m_fCooldown = 0.5;
	
	protected Widget m_wRoot;
	protected Widget m_wRankOverlay;
	protected Widget m_wSupplyOverlay;
	protected TextWidget m_wRankText;
	protected TextWidget m_wSupplyText;
	
	protected IEntity m_ArsenalEntity;
	protected IEntity m_CharacterEntity;
	protected SCR_ResourceComponent m_ArsenalResourceComponent;
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;
		m_wRankOverlay = w.FindAnyWidget("RankOverlay");
		m_wSupplyOverlay = w.FindAnyWidget("SupplyOverlay");
		m_wRankText = TextWidget.Cast(w.FindAnyWidget("RankText"));
		m_wSupplyText = TextWidget.Cast(w.FindAnyWidget("SupplyText"));
	}
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity arsenalEntity, IEntity characterEntity)
	{
		m_ArsenalEntity = arsenalEntity;
		m_CharacterEntity = characterEntity;
		
		m_ArsenalResourceComponent = SCR_ResourceComponent.FindResourceComponent(m_ArsenalEntity);
		
		RefreshRank();
		RefreshSupply();
	}
	
	//------------------------------------------------------------------------------------------------
	void Update(float tDelta)
	{
		m_fCooldown -= tDelta;
		if (m_fCooldown > 0)
			return;
		
		m_fCooldown = UPDATE_INTERVAL_SECONDS;
		
		RefreshRank();
		RefreshSupply();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RefreshRank()
	{
		if (!m_wRankOverlay || !m_CharacterEntity)
		{
			if (m_wRankOverlay)
				m_wRankOverlay.SetVisible(false);
			return;
		}
		
		SCR_ECharacterRank rank = SCR_CharacterRankComponent.GetCharacterRank(m_CharacterEntity);
		if (rank == SCR_ECharacterRank.INVALID)
		{
			m_wRankOverlay.SetVisible(false);
			return;
		}
		
		if (m_wRankText)
			m_wRankText.SetText(typename.EnumToString(SCR_ECharacterRank, rank));
		
		m_wRankOverlay.SetVisible(true);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RefreshSupply()
	{
		if (!m_wSupplyOverlay)
			return;
		
		// Check if resource system is enabled
		if (!SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled())
		{
			m_wSupplyOverlay.SetVisible(false);
			return;
		}
		
		// Get available resources using the correct helper method
		float supply;
		if (!SCR_ResourceSystemHelper.GetAvailableResources(m_ArsenalResourceComponent, supply))
		{
			m_wSupplyOverlay.SetVisible(false);
			return;
		}
		
		if (m_wSupplyText)
			m_wSupplyText.SetText(SCR_ResourceSystemHelper.SuppliesToString(supply));
		
		m_wSupplyOverlay.SetVisible(true);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current available supply for external checks
	float GetAvailableSupply()
	{
		float supply = 0;
		if (m_ArsenalResourceComponent)
			SCR_ResourceSystemHelper.GetAvailableResources(m_ArsenalResourceComponent, supply);
		return supply;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get player's current rank for external checks
	SCR_ECharacterRank GetPlayerRank()
	{
		if (!m_CharacterEntity)
			return SCR_ECharacterRank.INVALID;
		return SCR_CharacterRankComponent.GetCharacterRank(m_CharacterEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! Base class for editor option components
class PQD_EditorOptionComponent : ScriptedWidgetComponent
{
	protected string m_sOptionName;
	protected ref array<ref PQD_EditorOptionData> m_aChoices = {};
	protected int m_iSelectedIndex = 0;
	
	//------------------------------------------------------------------------------------------------
	string GetOptionName()
	{
		return m_sOptionName;
	}
	
	//------------------------------------------------------------------------------------------------
	void OnOptionSelected(string value)
	{
		// Override in subclasses
	}
	
	//------------------------------------------------------------------------------------------------
	int GetChoices(out array<PQD_EditorOptionData> outChoices)
	{
		foreach (PQD_EditorOptionData choice : m_aChoices)
		{
			outChoices.Insert(choice);
		}
		return m_aChoices.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	PQD_EditorOptionData GetSelectedOption()
	{
		if (m_iSelectedIndex < 0 || m_iSelectedIndex >= m_aChoices.Count())
			return null;
		return m_aChoices[m_iSelectedIndex];
	}
}
