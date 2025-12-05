// PQD Loadout Editor - Main Menu Class
// Author: PQD Team
// Version: 1.0.0

//------------------------------------------------------------------------------------------------
//! Main PQD Loadout Editor menu class
sealed class PQD_LoadoutEditor : ChimeraMenuBase
{
	static ref PQD_LoadoutEditor m_Instance;
	
	SCR_Faction m_arsenalFaction;
	InputManager m_pInputmanager;
	
	// Widgets
	Widget m_root;
	OverlayWidget m_wSlotChoices;
	PQD_SlotsUIComponent m_wSlotChoicesListbox;
	
	PQD_MultifunctionSlotUIComponent m_editedSlotInfo;
	Widget m_editedSlotInfoContainer;
	
	PQD_StatusMessageUIComponent m_wStatusMessageWidget;
	
	// Navigation buttons
	SCR_InputButtonComponent m_backButton;
	SCR_InputButtonComponent m_editButton;
	SCR_InputButtonComponent m_removeItemButton;
	SCR_InputButtonComponent m_loadLoadoutButton;
	SCR_InputButtonComponent m_saveLoadoutButton;
	SCR_InputButtonComponent m_clearLoadoutButton;

	// Current mode
	PQD_EditorMode m_eCurrentMode;
	PQD_InputMode m_eCurrentInputMode;
	
	// Entities
	IEntity m_EditedEntity;
	IEntity m_CharacterEntity;
	
	// Components
	SCR_InventoryStorageManagerComponent m_inventoryManager;
	IEntity m_ArsenalEntity;
	SCR_ArsenalComponent m_arsenalComponent;
	RplId m_ArsenalComponentRplId;
	
	// Slot history for navigation
	ref array<ref PQD_SlotInfo> m_slotHistory = {};
	
	// Cache
	ref PQD_Cache m_Cache = new PQD_Cache();
	
	// Player controller component
	PQD_PlayerControllerComponent m_pcComponent;
	
	// UI state
	bool m_bIsActionInProgress = false;
	int m_iListBoxLastActionChild = -1;
	bool m_bLastFocusWasInventoryPanel = false; // Track which panel was last clicked

	// UI components
	PQD_PreviewUIComponent m_wPreviewWidgetComponent;
	PQD_CategoryButtonsUIComponent m_SlotCategoryWidgetComponent;
	PQD_InventoryPanelUIComponent m_InventoryPanelWidgetComponent;
	PQD_RankAndSupplyInfoComponent m_RankAndSupplyComponent;
	
	static bool m_bIntroDialogConfirmed = false;
	
	// Editor options
	static ref map<string, PQD_EditorOptionComponent> m_EditorOptions = new map<string, PQD_EditorOptionComponent>;

	//------------------------------------------------------------------------------------------------
	void OnCriticalError(PQD_ProcessingStep step, string reason, string appendData = "")
	{
		string debugInformation = "";
		
		switch (step)
		{
			case PQD_ProcessingStep.MENU_OPEN:
				debugInformation += string.Format("m_root: %1\n", m_root);
				debugInformation += string.Format("m_wSlotChoices: %1\n", m_wSlotChoices);
				debugInformation += string.Format("m_wSlotChoicesListbox: %1\n", m_wSlotChoicesListbox);
				break;
			case PQD_ProcessingStep.MENU_INIT:
				debugInformation += string.Format("m_CharacterEntity: %1\n", m_CharacterEntity);
				debugInformation += string.Format("m_ArsenalEntity: %1\n", m_ArsenalEntity);
				debugInformation += string.Format("m_inventoryManager: %1\n", m_inventoryManager);
				break;
		}
		
		if (!appendData.IsEmpty())
			debugInformation += appendData;
		
		Print(string.Format("[PQD] Critical error: %1\n%2", reason, debugInformation), LogLevel.ERROR);

		SCR_ConfigurableDialogUi dialog = PQD_Helpers.CreateDialog("error", 
			string.Format("A critical error occurred and this menu will close.\n\nReason: %1", reason), debugInformation);

		if (dialog)
			dialog.m_OnConfirm.Insert(Destroy);
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowWarning(string message, string additionalData = "")
	{
		Print(string.Format("[PQD] Warning: %1\n%2", message, additionalData), LogLevel.WARNING);
		SCR_ConfigurableDialogUi dialog = PQD_Helpers.CreateDialog("warning", message, additionalData);
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowIntroDialog()
	{
		if (m_bIntroDialogConfirmed)
			return;

		// Skip intro dialog for now - will show when config is properly set up
		m_bIntroDialogConfirmed = true;
		
		/*
		SCR_ConfigurableDialogUi dialog = PQD_Helpers.CreateDialog("intro", 
			"PQD Loadout Editor v1.0\n\nUse this editor to customize your character's equipment and save loadouts.");
		if (dialog)
			dialog.m_OnConfirm.Insert(OnIntroDialogConfirm);
		*/
	}
	
	//------------------------------------------------------------------------------------------------
	void OnIntroDialogConfirm()
	{
		m_bIntroDialogConfirmed = true;
	}
	
	//------------------------------------------------------------------------------------------------
	static PQD_LoadoutEditor GetInstance()
	{
		return m_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterOption(string optionName, PQD_EditorOptionComponent optionComponent)
	{
		if (m_EditorOptions.Contains(optionName))
		{
			Print(string.Format("[PQD] Option %1 already registered", optionName), LogLevel.ERROR);
			return;
		}
		
		m_EditorOptions.Set(optionName, optionComponent);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuInit()
	{
		if (!m_Instance)
			m_Instance = this;
		
		m_EditorOptions.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_root = GetRootWidget();
		if (!m_root)
		{
			OnCriticalError(PQD_ProcessingStep.MENU_OPEN, "Root widget is null");
			return;
		}
		
		// Force activate context
		InputManager localInputManager = GetGame().GetInputManager();
		if (localInputManager)
		{
			localInputManager.ActivateContext("PQD_LoadoutEditor");
			Print("[PQD] Activated Context: PQD_LoadoutEditor", LogLevel.NORMAL);
		}
		
		// Slot choices listbox
		m_wSlotChoices = OverlayWidget.Cast(m_root.FindAnyWidget("ListBoxSlotChoices"));
		if (m_wSlotChoices)
			m_wSlotChoicesListbox = PQD_SlotsUIComponent.Cast(m_wSlotChoices.FindHandler(PQD_SlotsUIComponent));
		
		// Current slot indicator
		Widget editedSlotWidget = m_root.FindAnyWidget("EditedSlotInfo");
		if (editedSlotWidget)
			m_editedSlotInfo = PQD_MultifunctionSlotUIComponent.Cast(editedSlotWidget.FindHandler(PQD_MultifunctionSlotUIComponent));
		m_editedSlotInfoContainer = m_root.FindAnyWidget("EditedSlotInfoContainer");
		
		// Status message
		Widget statusWidget = m_root.FindAnyWidget("Statusbar");
		if (statusWidget)
			m_wStatusMessageWidget = PQD_StatusMessageUIComponent.Cast(statusWidget.FindHandler(PQD_StatusMessageUIComponent));
		
		if (!m_wSlotChoices || !m_wSlotChoicesListbox)
		{
			OnCriticalError(PQD_ProcessingStep.MENU_OPEN, "Required widgets not found");
			return;
		}
		
		// Buttons
		Widget footer = m_root.FindAnyWidget("Footer");
		
		if (footer)
		{
			m_backButton = SCR_InputButtonComponent.GetInputButtonComponent("BackButton", footer);
			m_editButton = SCR_InputButtonComponent.GetInputButtonComponent("EditButton", footer);
			m_removeItemButton = SCR_InputButtonComponent.GetInputButtonComponent("RemoveItemButton", footer);
			m_loadLoadoutButton = SCR_InputButtonComponent.GetInputButtonComponent("LoadLoadoutButton", footer);
			m_saveLoadoutButton = SCR_InputButtonComponent.GetInputButtonComponent("SaveLoadoutButton", footer);
			m_clearLoadoutButton = SCR_InputButtonComponent.GetInputButtonComponent("ClearLoadoutButton", footer);
			
			if (m_backButton)
				m_backButton.m_OnActivated.Insert(OnButtonPressed_Back);
			if (m_editButton)
				m_editButton.m_OnActivated.Insert(OnButtonPressed_Edit);
			if (m_removeItemButton)
				m_removeItemButton.m_OnActivated.Insert(OnButtonPressed_RemoveItem);
			if (m_loadLoadoutButton)
				m_loadLoadoutButton.m_OnActivated.Insert(OnButtonPressed_LoadLoadout);
			if (m_saveLoadoutButton)
				m_saveLoadoutButton.m_OnActivated.Insert(OnButtonPressed_SaveLoadout);
			if (m_clearLoadoutButton)
				m_clearLoadoutButton.m_OnActivated.Insert(OnButtonPressed_ClearLoadout);
		}

		if (m_wSlotChoicesListbox)
		{
			m_wSlotChoicesListbox.m_OnItemClicked.Insert(OnEditedSlotChanged);
			m_wSlotChoicesListbox.m_OnItemFocused.Insert(OnSlotFocusChanged);
			m_wSlotChoicesListbox.m_OnPanelFocused.Insert(OnSlotsPanelFocused);
		}

		// Preview widget
		Widget previewWidget = m_root.FindAnyWidget("ItemPreviewContainer");
		if (previewWidget)
		{
			m_wPreviewWidgetComponent = PQD_PreviewUIComponent.Cast(previewWidget.FindHandler(PQD_PreviewUIComponent));
			
			// Connect slot hotspot click callback
			if (m_wPreviewWidgetComponent)
				m_wPreviewWidgetComponent.GetOnSlotHotspotClicked().Insert(OnSlotHotspotClicked);
		}
		
		// Category selector
		Widget categoryWidget = m_root.FindAnyWidget("CategorySelector");
		if (categoryWidget)
		{
			m_SlotCategoryWidgetComponent = PQD_CategoryButtonsUIComponent.Cast(categoryWidget.FindHandler(PQD_CategoryButtonsUIComponent));
			if (m_SlotCategoryWidgetComponent)
				m_SlotCategoryWidgetComponent.m_OnCategoryChangedInvoker.Insert(OnSlotCategoryChanged);
		}
		
		// Inventory panel
		Widget inventoryPanel = m_root.FindAnyWidget("InventoryPanelMain");
		if (inventoryPanel)
		{
			m_InventoryPanelWidgetComponent = PQD_InventoryPanelUIComponent.Cast(inventoryPanel.FindHandler(PQD_InventoryPanelUIComponent));
			if (m_InventoryPanelWidgetComponent)
			{
				m_InventoryPanelWidgetComponent.m_OnItemClicked.Insert(OnItemAddRequested);
				m_InventoryPanelWidgetComponent.m_OnItemFocused.Insert(OnInventoryPanelItemFocusChanged);
			}
		}
		
		// Register action listeners for tab navigation (E key only)
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.AddActionListener("PQD_TabNext", EActionTrigger.DOWN, Action_TabNext);
			// Block Q key from closing menu - this action consumes Q before other contexts can
			inputManager.AddActionListener("PQD_BlockQ", EActionTrigger.DOWN, Action_BlockQ);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Action listener callback for tab navigation (E key cycles through tabs)
	protected void Action_TabNext()
	{
		if (m_SlotCategoryWidgetComponent)
			m_SlotCategoryWidgetComponent.SelectNextCategory();
	}
	
	//------------------------------------------------------------------------------------------------
	// Action listener callback to consume Q key (prevents other contexts from receiving it)
	protected void Action_BlockQ()
	{
		// Intentionally empty - this just consumes the Q key press
		// so it doesn't reach InventoryContext or other lower-priority contexts
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		if (m_wPreviewWidgetComponent)
			m_wPreviewWidgetComponent.Update(tDelta);
		if (m_wStatusMessageWidget)
			m_wStatusMessageWidget.Update(tDelta);
		if (m_RankAndSupplyComponent)
			m_RankAndSupplyComponent.Update(tDelta);
		
		if (!m_pInputmanager)
			return;
		
		// Handle ESC key for back navigation (only ESC/Gamepad B, NOT Q)
		if (m_pInputmanager.GetActionTriggered("MenuBack"))
		{
			OnEscapePressed();
			return;
		}
		
		// Handle F key for editing storage items  
		if (m_pInputmanager.GetActionTriggered("PQD_Edit"))
		{
			OnButtonPressed_Edit();
			return;
		}
		
		// Handle X key / Gamepad X for saving loadout
		if (m_pInputmanager.GetActionTriggered("PQD_SaveLoadout"))
		{
			OnInputAction_SaveLoadout();
			return;
		}
		
		// Handle Enter key / Gamepad A for loading loadout
		if (m_pInputmanager.GetActionTriggered("PQD_LoadLoadout"))
		{
			OnInputAction_LoadLoadout();
			return;
		}
		
		// Handle Delete key / Gamepad B for clearing loadout
		if (m_pInputmanager.GetActionTriggered("PQD_ClearLoadout"))
		{
			OnInputAction_ClearLoadout();
			return;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle Save Loadout input action (X key / Gamepad X)
	protected void OnInputAction_SaveLoadout()
	{
		if (IsUIWaiting())
			return;
		
		// Only works in loadout modes
		if (m_eCurrentMode != PQD_EditorMode.LOADOUTS && m_eCurrentMode != PQD_EditorMode.SERVER_LOADOUTS)
			return;
		
		if (!m_wSlotChoicesListbox)
			return;
		
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		PQD_SlotLoadout loadoutSlot = PQD_SlotLoadout.Cast(data);
		if (!loadoutSlot)
			return;
		
		// Save loadout - use button code 1 for normal, 3 for admin
		if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 1);  // Save normal loadout
		else if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 3);  // Save admin loadout
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle Load Loadout input action (Enter key / Gamepad A)
	protected void OnInputAction_LoadLoadout()
	{
		if (IsUIWaiting())
			return;
		
		// Only works in loadout modes
		if (m_eCurrentMode != PQD_EditorMode.LOADOUTS && m_eCurrentMode != PQD_EditorMode.SERVER_LOADOUTS)
			return;
		
		if (!m_wSlotChoicesListbox)
			return;
		
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		PQD_SlotLoadout loadoutSlot = PQD_SlotLoadout.Cast(data);
		if (!loadoutSlot)
			return;
		
		// Check if slot has data
		if (!loadoutSlot.HasData())
		{
			if (m_wStatusMessageWidget)
				m_wStatusMessageWidget.ShowMessage("This slot is empty", PQD_MessageType.WARNING);
			PQD_Helpers.PlaySound("blocked");
			return;
		}
		
		// Load loadout - use button code 0 for normal, 2 for admin
		if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 0);  // Load normal loadout
		else if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 2);  // Load admin loadout
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle Clear Loadout input action (Delete key / Gamepad B)
	protected void OnInputAction_ClearLoadout()
	{
		if (IsUIWaiting())
			return;
		
		// Only works in loadout modes
		if (m_eCurrentMode != PQD_EditorMode.LOADOUTS && m_eCurrentMode != PQD_EditorMode.SERVER_LOADOUTS)
			return;
		
		if (!m_wSlotChoicesListbox)
			return;
		
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		PQD_SlotLoadout loadoutSlot = PQD_SlotLoadout.Cast(data);
		if (!loadoutSlot)
			return;
		
		// Check if slot has data to clear
		if (!loadoutSlot.HasData())
		{
			if (m_wStatusMessageWidget)
				m_wStatusMessageWidget.ShowMessage("This slot is already empty", PQD_MessageType.WARNING);
			PQD_Helpers.PlaySound("blocked");
			return;
		}
		
		// Clear loadout - use button code 100 for normal, 101 for admin
		if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 100);  // Clear normal loadout
		else if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
			HandleChangedLoadoutOption(loadoutSlot, 101);  // Clear admin loadout
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnEscapePressed()
	{
		// If we're in a sub-menu (options/storage), go back
		if (m_slotHistory && m_slotHistory.Count() > 0)
		{
			OnButtonPressed_Back();
			return;
		}
		
		// If we're in main menu, close the editor
		Close();
	}
	
	//------------------------------------------------------------------------------------------------
	void TryUnequipHeldItem(IEntity character)
	{
		SCR_InventoryStorageManagerComponent storageManager = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!storageManager)
			return;

		SCR_CharacterInventoryStorageComponent storage = storageManager.GetCharacterStorage();
		if (storage)
			storage.UnequipCurrentItem();
	}

	//------------------------------------------------------------------------------------------------
	// Main initialization when menu is accessed via action
	void Init(IEntity arsenalEntity, IEntity characterEntity)
	{
		m_CharacterEntity = characterEntity;
		m_ArsenalEntity = arsenalEntity;
		
		if (!characterEntity || !arsenalEntity)
		{
			OnCriticalError(PQD_ProcessingStep.MENU_INIT, "Null entity provided");
			return;
		}
		
		TryUnequipHeldItem(characterEntity);
		
		m_inventoryManager = SCR_InventoryStorageManagerComponent.Cast(characterEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		m_arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		m_ArsenalComponentRplId = Replication.FindId(m_arsenalComponent);
		
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(arsenalEntity.FindComponent(FactionAffiliationComponent));
		m_pcComponent = PQD_Helpers.GetPlayerControllerComponent();
		m_pInputmanager = GetGame().GetInputManager();
		
		// Debug component check
		string missingComponents = "";
		if (!m_inventoryManager) missingComponents += "InventoryManager, ";
		if (!m_arsenalComponent) missingComponents += "ArsenalComponent, ";
		if (!m_pcComponent) missingComponents += "PQD_PlayerControllerComponent, ";
		if (!m_pInputmanager) missingComponents += "InputManager, ";
		
		if (missingComponents != "")
		{
			OnCriticalError(PQD_ProcessingStep.MENU_INIT, "Missing: " + missingComponents);
			return;
		}
		
		if (factionComp)
			m_arsenalFaction = SCR_Faction.Cast(factionComp.GetAffiliatedFaction());
		
		m_pcComponent.m_OnResponse_Storage.Insert(OnServerResponse_Storage);
		m_pcComponent.m_OnResponse_Loadout.Insert(OnServerResponse_Loadout);
		
		if (!m_Cache.Init(m_arsenalComponent))
			ShowWarning("This arsenal seems to have no items!");
		
		// Start in Character mode
		SetUIWaiting(true);
		SetLoadoutEditorMode(PQD_EditorMode.CHARACTER);
		
		if (m_wPreviewWidgetComponent)
		{
			m_wPreviewWidgetComponent.Init(m_CharacterEntity);
			m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, m_CharacterEntity, null);
		}
		
		SCR_PlayerController.Cast(GetGame().GetPlayerController()).m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);
		
		// Rank and supply info
		Widget rankSupplyWidget = m_root.FindAnyWidget("RankAndSupplyInfo");
		if (rankSupplyWidget)
		{
			m_RankAndSupplyComponent = PQD_RankAndSupplyInfoComponent.Cast(rankSupplyWidget.FindHandler(PQD_RankAndSupplyInfoComponent));
			if (m_RankAndSupplyComponent)
				m_RankAndSupplyComponent.Init(m_ArsenalEntity, m_CharacterEntity);
		}
		
		ShowIntroDialog();
	}
	
	//------------------------------------------------------------------------------------------------
	void DelayedUpdatePlayerCharacter()
	{
		if (m_wPreviewWidgetComponent)
		{
			m_wPreviewWidgetComponent.Init(m_CharacterEntity);
			m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, m_CharacterEntity, null);
		}
		
		if (m_RankAndSupplyComponent)
			m_RankAndSupplyComponent.Init(m_ArsenalEntity, m_CharacterEntity);
		
		if (IsUIWaiting())
			SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		if (!to)
		{
			Destroy();
			return;
		}
		
		Print(string.Format("[PQD] New player character entity: %1", to), LogLevel.DEBUG);

		m_CharacterEntity = to;
		GetGame().GetCallqueue().CallLater(DelayedUpdatePlayerCharacter, 500, false);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetUIWaiting(bool state)
	{
		m_bIsActionInProgress = state;
		
		if (state == false && m_iListBoxLastActionChild > -1)
		{
			if (m_wSlotChoicesListbox)
				m_wSlotChoicesListbox.SetFocus(m_iListBoxLastActionChild);
			m_iListBoxLastActionChild = -1;
		}
		
		if (m_wSlotChoices)
			m_wSlotChoices.SetEnabled(!state);
		if (m_SlotCategoryWidgetComponent)
			m_SlotCategoryWidgetComponent.SetEnabled(!state);
		if (m_editButton)
			m_editButton.SetEnabled(!state);
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsUIWaiting()
	{
		return m_bIsActionInProgress;
	}

	//------------------------------------------------------------------------------------------------
	private PQD_SlotInfo GetCurrentlyEditedSlot()
	{
		if (m_slotHistory.Count() == 0)
			return null;
		
		return m_slotHistory.Get(m_slotHistory.Count() - 1);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnButtonPressed_Back()
	{
		if (!IsFocused() || IsUIWaiting())
			return;

		int historyCount = m_slotHistory.Count();

		// In STORAGE mode (Items tab), handle navigation based on which panel has focus
		if (m_eCurrentMode == PQD_EditorMode.STORAGE)
		{
			// If Available Items panel has focus
			if (m_bLastFocusWasInventoryPanel)
			{
				// If in submenu (viewing a category), go back to category menu
				if (m_InventoryPanelWidgetComponent && m_InventoryPanelWidgetComponent.IsInSubMenu())
				{
					m_InventoryPanelWidgetComponent.ExitSubMenu();
					ListAllArsenalItemsForPanel();
					return;
				}
				// If at category menu and there's history, hide panel and focus main list
				else if (historyCount > 0)
				{
					m_bLastFocusWasInventoryPanel = false;
					if (m_wSlotChoicesListbox)
						m_wSlotChoicesListbox.SetFocus(0);
					return;
				}
			}
			// If main inventory list has focus
			else
			{
				// If there's history in main list, go back in history
				if (historyCount > 0)
				{
					int focusWanted = m_slotHistory[historyCount - 1].listBoxChildId;
					historyCount -= 1;
					m_slotHistory.Remove(historyCount);

					if (m_slotHistory.Count() == 0)
					{
						// Back to main container list
						SetLoadoutEditorMode(PQD_EditorMode.STORAGE);
						if (m_wSlotChoicesListbox)
							m_wSlotChoicesListbox.SetFocus(focusWanted);
						return;
					}

					SetViewingStorageSlots(m_slotHistory[historyCount - 1]);
					UpdateEditedSlotInfo();
					CreateSlotsForCurrentEntity();

					if (m_wSlotChoicesListbox)
						m_wSlotChoicesListbox.SetFocus(focusWanted);
					return;
				}
				// If no history in main list, switch focus to Available Items
				else if (m_InventoryPanelWidgetComponent && m_InventoryPanelWidgetComponent.GetRootWidget().IsVisible())
				{
					m_bLastFocusWasInventoryPanel = true;
					// Focus the inventory panel programmatically if possible
					return;
				}
			}
		}

		// Old submenu check for non-STORAGE modes
		if (m_InventoryPanelWidgetComponent && m_InventoryPanelWidgetComponent.IsInSubMenu())
		{
			m_InventoryPanelWidgetComponent.ExitSubMenu();
			ListAllArsenalItemsForPanel();
			return;
		}

		// Go back in slots (for non-STORAGE modes)
		if (historyCount > 0)
		{
			if (m_InventoryPanelWidgetComponent && m_InventoryPanelWidgetComponent.GetRootWidget().IsVisible())
			{
				m_InventoryPanelWidgetComponent.HidePanel();
				if (m_wSlotChoicesListbox)
					m_wSlotChoicesListbox.SetFocus(0);
				return;
			}
			
			int focusWanted = m_slotHistory[historyCount - 1].listBoxChildId;
			
			historyCount -= 1;
			m_slotHistory.Remove(historyCount);

			if (m_slotHistory.Count() == 0)
			{
				if (m_eCurrentMode == PQD_EditorMode.STORAGE)
					SetLoadoutEditorMode(PQD_EditorMode.STORAGE);
				else if (m_eCurrentMode == PQD_EditorMode.OPTIONS)
					SetLoadoutEditorMode(PQD_EditorMode.OPTIONS);
				else
					SetLoadoutEditorMode(PQD_EditorMode.CHARACTER);
				
				if (m_wSlotChoicesListbox)
					m_wSlotChoicesListbox.SetFocus(focusWanted);
				return;
			}
			
			SetViewingStorageSlots(m_slotHistory[historyCount - 1]);
			
			UpdateEditedSlotInfo();
			CreateSlotsForCurrentEntity();
			
			if (m_wSlotChoicesListbox)
				m_wSlotChoicesListbox.SetFocus(focusWanted);
			
			return;
		}

		Destroy();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		// Remove action listeners
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener("PQD_TabNext", EActionTrigger.DOWN, Action_TabNext);
			inputManager.RemoveActionListener("PQD_BlockQ", EActionTrigger.DOWN, Action_BlockQ);
		}
		
		Destroy();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMenuHide()
	{
		Destroy();
	}

	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_Swap()
	{
		if (IsUIWaiting())
			return;
		
		if (m_pInputmanager.GetLastUsedInputDevice() == EInputDeviceType.GAMEPAD)
			return;

		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.SetFocusedItemSelected();
	}
	
	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_RemoveItem()
	{
		if (IsUIWaiting())
			return;

		if (!m_wSlotChoicesListbox)
			return;
			
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());

		if (m_eCurrentMode == PQD_EditorMode.STORAGE && PQD_SlotInfoStorageItem.Cast(data))
		{
			SetUIWaiting(true);
			HandleClickedInventoryItem(PQD_SlotInfoStorageItem.Cast(data), 1);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_SaveLoadout()
	{
		if (IsUIWaiting())
			return;

		if (!m_wSlotChoicesListbox)
			return;
			
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		
		if (PQD_SlotLoadout.Cast(data))
		{
			if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
				HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), 1);
			if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
				HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), 3);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_LoadLoadout()
	{
		if (IsUIWaiting())
			return;

		if (!m_wSlotChoicesListbox)
			return;
			
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		
		if (PQD_SlotLoadout.Cast(data))
		{
			PQD_SlotLoadout loadoutSlot = PQD_SlotLoadout.Cast(data);
			
			// Check if slot has data
			if (loadoutSlot.metadataWeapons.IsEmpty())
			{
				if (m_wStatusMessageWidget)
					m_wStatusMessageWidget.ShowMessage("This slot is empty", PQD_MessageType.WARNING);
				PQD_Helpers.PlaySound("blocked");
				return;
			}
			
			if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
				HandleChangedLoadoutOption(loadoutSlot, 0);  // Load normal loadout
			if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
				HandleChangedLoadoutOption(loadoutSlot, 2);  // Load admin loadout
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_ClearLoadout()
	{
		if (IsUIWaiting())
			return;

		if (!m_wSlotChoicesListbox)
			return;
			
		Managed data = m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem());
		
		if (PQD_SlotLoadout.Cast(data))
		{
			if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
				HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), 100);
			if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
				HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), 101);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnButtonPressed_Edit()
	{
		Print("[PQD] OnButtonPressed_Edit: Called", LogLevel.NORMAL);
		
		if (IsUIWaiting())
		{
			Print("[PQD] OnButtonPressed_Edit: UI is waiting, aborting", LogLevel.DEBUG);
			return;
		}

		if (!m_wSlotChoicesListbox)
		{
			Print("[PQD] OnButtonPressed_Edit: No slot choices listbox", LogLevel.DEBUG);
			return;
		}
		
		int focusedItem = m_wSlotChoicesListbox.GetFocusedItem();
		if (focusedItem < 0)
		{
			Print("[PQD] OnButtonPressed_Edit: No focused item", LogLevel.DEBUG);
			return;
		}
		
		Managed itemData = m_wSlotChoicesListbox.GetItemData(focusedItem);
		
		// Only PQD_SlotInfo items can be edited - cast directly like Bacon does
		PQD_SlotInfo slotInfo = PQD_SlotInfo.Cast(itemData);
		if (!slotInfo)
		{
			Print("[PQD] OnButtonPressed_Edit: Focused item is not a PQD_SlotInfo", LogLevel.DEBUG);
			return;
		}
		
		// Get attached entity
		IEntity ent = slotInfo.slot.GetAttachedEntity();
		
		// Edit only works for items with storage (weapons with attachments, containers)
		if (!ent || !slotInfo.hasStorage)
		{
			Print(string.Format("[PQD] OnButtonPressed_Edit: Cannot edit - ent=%1, hasStorage=%2", ent, slotInfo.hasStorage), LogLevel.DEBUG);
			return;
		}
		
		Print(string.Format("[PQD] OnButtonPressed_Edit: Entering edit mode for %1", slotInfo.slotName), LogLevel.NORMAL);
		m_slotHistory.Insert(slotInfo);
		SetViewingStorageSlots(slotInfo);
	}

	//------------------------------------------------------------------------------------------------
	void SetViewingStorageSlots(PQD_SlotInfo slotInfo)
	{
		if (!m_wPreviewWidgetComponent)
			return;
		
		// Handle STORAGE mode (Items tab) - when editing a container, show arsenal items
		if (m_eCurrentMode == PQD_EditorMode.STORAGE)
		{
			// Show the inventory panel with arsenal items for adding
			if (m_InventoryPanelWidgetComponent)
			{
				m_InventoryPanelWidgetComponent.ShowPanel();
				// Make sure we reset to category menu when entering storage editing
				m_InventoryPanelWidgetComponent.ExitSubMenu();
				ListAllArsenalItemsForPanel();
			}
			// List current inventory contents in the main list
			ListItemsForStorage();
			return;
		}
			
		if (slotInfo.slotType == PQD_SlotType.CHARACTER_LOADOUT)
		{
			SetLoadoutEditorMode(PQD_EditorMode.ENTITY, slotInfo.slot.GetAttachedEntity());
			m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, slotInfo.slot.GetAttachedEntity(), null);
			return;
		}

		if (slotInfo.slotType == PQD_SlotType.CHARACTER_WEAPON)
		{
			SetLoadoutEditorMode(PQD_EditorMode.ENTITY, slotInfo.slot.GetAttachedEntity());
			m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.ENTITY, m_slotHistory[0].slot.GetAttachedEntity(), null);
			return;
		}
		
		if (slotInfo.slotType == PQD_SlotType.ATTACHMENT)
		{
			SetLoadoutEditorMode(PQD_EditorMode.ENTITY, slotInfo.slot.GetAttachedEntity());
			m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.ENTITY, m_slotHistory[0].slot.GetAttachedEntity(), slotInfo);
			return;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnSlotCategoryChanged()
	{
		// Clear slot history when switching main categories
		if (m_slotHistory.Count() > 0)
		{
			m_slotHistory.Clear();
			Print("[PQD] Cleared slot history on category change", LogLevel.DEBUG);
		}
		
		if (!m_SlotCategoryWidgetComponent)
			return;
		
		// Clear the list first
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
			
		PQD_SlotCategory newCategory = m_SlotCategoryWidgetComponent.GetCurrentCategoryCharacter();
		
		Print(string.Format("[PQD] OnSlotCategoryChanged - New category: %1", newCategory), LogLevel.DEBUG);
		
		SetRemoveItemButtonActive(false);
		SetLoadoutButtonsActive(false);
		
		switch (newCategory)
		{
			case PQD_SlotCategory.CHARACTER_CLOTHING:
			case PQD_SlotCategory.CHARACTER_GEAR:
				if (m_editButton)
					m_editButton.SetLabel("Edit");
				SetLoadoutEditorMode(PQD_EditorMode.CHARACTER, m_CharacterEntity);
				break;
			case PQD_SlotCategory.CHARACTER_ITEMS:
				SetLoadoutEditorMode(PQD_EditorMode.STORAGE, m_CharacterEntity);
				break;
			case PQD_SlotCategory.SETTINGS:
				SetLoadoutEditorMode(PQD_EditorMode.OPTIONS, m_CharacterEntity);
				break;
			case PQD_SlotCategory.SERVER_LOADOUTS:
				SetUIWaiting(true);
				SetLoadoutEditorMode(PQD_EditorMode.SERVER_LOADOUTS, m_CharacterEntity);
				break;
			case PQD_SlotCategory.LOADOUTS:
				SetUIWaiting(true);
				SetLoadoutEditorMode(PQD_EditorMode.LOADOUTS, m_CharacterEntity);
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnEditedSlotChanged(PQD_SlotsUIComponent comp, Managed data, int button)
	{
		if (IsUIWaiting())
			return;

		// Inventory mode - clicked inventory item
		if (m_eCurrentMode == PQD_EditorMode.STORAGE && PQD_SlotInfoStorageItem.Cast(data))
		{
			HandleClickedInventoryItem(PQD_SlotInfoStorageItem.Cast(data), button);
			return;
		}
		
		if (m_eCurrentMode == PQD_EditorMode.OPTIONS && m_slotHistory.Count() > 0)
		{
			HandleChangedEditorOption(PQD_EditorOptionData.Cast(data));
			return;
		}
		
		if (m_eCurrentMode == PQD_EditorMode.LOADOUTS)
		{
			HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), button);
			return;
		}
		
		if (m_eCurrentMode == PQD_EditorMode.SERVER_LOADOUTS)
		{
			HandleChangedLoadoutOption(PQD_SlotLoadout.Cast(data), button + 2);
			return;
		}
				
		PQD_SlotInfo slotInfo = PQD_SlotInfo.Cast(data);
		if (slotInfo)
		{
			HandleChangedSlot(slotInfo);
			return;
		}

		PQD_SlotChoiceIdentity slotChoiceIdentity = PQD_SlotChoiceIdentity.Cast(data);
		if (slotChoiceIdentity)
		{
			HandleChangedIdentity(slotChoiceIdentity);
			return;
		}
		
		PQD_SlotChoice slotChoice = PQD_SlotChoice.Cast(data);
		if (slotChoice)
		{
			HandleChangedOption(slotChoice);
			return;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleClickedInventoryItem(PQD_SlotInfoStorageItem itemInfo, int button)
	{
		PQD_StorageRequest request = new PQD_StorageRequest();

		request.storageRplId = m_Cache.GetStorageRplId(itemInfo.slot.GetStorage());
		request.arsenalEntityRplId = PQD_Helpers.GetEntityRplId(m_ArsenalEntity);
		
		if (m_wSlotChoicesListbox)
			m_iListBoxLastActionChild = m_wSlotChoicesListbox.GetItem(m_wSlotChoicesListbox.GetFocusedItem()).GetZOrder();

		switch (button)
		{
			case 0:
				request.storageSlotId = -1;
				request.actionType = PQD_ActionType.ADD_ITEM;
				PQD_Helpers.GetResourceNameFromEntity(itemInfo.slot.GetAttachedEntity(), request.prefab);
				break;
			case 1:
				request.storageSlotId = itemInfo.storageSlotId;
				request.actionType = PQD_ActionType.REMOVE_ITEM;
				break;
		}

		m_pcComponent.RequestAction(request);
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleChangedEditorOption(PQD_EditorOptionData selected)
	{
		PQD_EditorOptionComponent optionComponent = m_EditorOptions.Get(selected.editorOptionLabel);
		if (optionComponent)
			optionComponent.OnOptionSelected(selected.optionValue);
		
		if (m_wSlotChoicesListbox)
			m_iListBoxLastActionChild = m_wSlotChoicesListbox.GetFocusedItem();
		
		if (m_editedSlotInfo)
			m_editedSlotInfo.SetData(selected);
		CreateOptionsForSlot();
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleChangedSlot(PQD_SlotInfo slotInfo)
	{
		if (!slotInfo.slotEnabled)
		{
			PQD_Helpers.PlaySound("blocked");
			SetUIWaiting(false);
			return;
		}

		m_slotHistory.Insert(slotInfo);

		// In STORAGE mode (Items tab), when clicking a container, show arsenal items panel
		if (m_eCurrentMode == PQD_EditorMode.STORAGE && slotInfo.hasStorage)
		{
			// Show inventory panel with arsenal items (but don't reload - keep current view)
			if (m_InventoryPanelWidgetComponent && !m_InventoryPanelWidgetComponent.GetRootWidget().IsVisible())
			{
				// Only show panel if it's not already visible
				m_InventoryPanelWidgetComponent.ShowPanel();
			}
			// List current storage contents in main list
			ListItemsForStorage();
			SetUIWaiting(false);
			return;
		}

		if (m_wPreviewWidgetComponent)
		{
			switch (slotInfo.slotType)
			{
				case PQD_SlotType.MAGAZINE:
				case PQD_SlotType.ATTACHMENT:
					m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.ENTITY, m_slotHistory[0].slot.GetAttachedEntity(), slotInfo);
					break;
				case PQD_SlotType.CHARACTER_LOADOUT:
					if (m_eCurrentMode == PQD_EditorMode.STORAGE)
						m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.ENTITY, m_slotHistory[0].slot.GetAttachedEntity(), slotInfo);
					else
						m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, m_CharacterEntity, slotInfo);
					break;
				case PQD_SlotType.OPTION:
					break;
				default:
					m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, m_CharacterEntity, slotInfo);
					break;
			}
		}
		
		UpdateEditedSlotInfo();
		CreateOptionsForSlot();
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetEditButtonActive(bool active)
	{
		Print(string.Format("[PQD] SetEditButtonActive: active=%1, m_editButton=%2", active, m_editButton), LogLevel.NORMAL);
		
		if (!m_editButton)
		{
			Print("[PQD] SetEditButtonActive: m_editButton is null!", LogLevel.WARNING);
			return;
		}
		
		if (active == m_editButton.IsVisible())
			return;

		m_editButton.SetEnabled(active);
		m_editButton.SetVisible(active);
		Print(string.Format("[PQD] SetEditButtonActive: Button now enabled=%1, visible=%2", m_editButton.IsEnabled(), m_editButton.IsVisible()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnSlotFocusChanged(PQD_SlotsUIComponent component, Managed data)
	{
		// Mark that main inventory panel has focus (not the Available Items panel)
		m_bLastFocusWasInventoryPanel = false;

		Print(string.Format("[PQD] OnSlotFocusChanged: data type=%1", data), LogLevel.NORMAL);

		if (PQD_SlotInfoStorageItem.Cast(data))
		{
			SetRemoveItemButtonActive(true);
			return;
		}

		SetRemoveItemButtonActive(false);

		PQD_SlotInfo slotInfo = PQD_SlotInfo.Cast(data);
		if (!slotInfo)
		{
			Print("[PQD] OnSlotFocusChanged: data is not PQD_SlotInfo", LogLevel.DEBUG);
			SetEditButtonActive(false);
			return;
		}

		Print(string.Format("[PQD] OnSlotFocusChanged: slot=%1, hasStorage=%2", slotInfo.slotName, slotInfo.hasStorage), LogLevel.NORMAL);
		SetEditButtonActive(slotInfo.hasStorage);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetRemoveItemButtonActive(bool active)
	{
		if (!m_removeItemButton || m_removeItemButton.IsEnabled() == active)
			return;
		
		m_removeItemButton.SetEnabled(active);
		m_removeItemButton.SetVisible(active);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLoadoutButtonsActive(bool active)
	{
		if (m_loadLoadoutButton)
		{
			m_loadLoadoutButton.SetEnabled(active);
			m_loadLoadoutButton.SetVisible(active);
		}
		
		if (m_saveLoadoutButton)
		{
			m_saveLoadoutButton.SetEnabled(active);
			m_saveLoadoutButton.SetVisible(active);
		}
		
		if (m_clearLoadoutButton)
		{
			m_clearLoadoutButton.SetEnabled(active);
			m_clearLoadoutButton.SetVisible(active);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void Destroy()
	{
		SCR_PlayerController.Cast(GetGame().GetPlayerController()).m_OnControlledEntityChanged.Remove(OnControlledEntityChanged);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		menuManager.CloseMenuByPreset(ChimeraMenuPreset.PQD_LoadoutEditor);
		m_Instance = null;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLoadoutEditorMode(PQD_EditorMode newMode, IEntity editedEntity = null)
	{
		m_eCurrentMode = newMode;
		
		if (m_InventoryPanelWidgetComponent)
			m_InventoryPanelWidgetComponent.HidePanel();
		
		if (m_editButton)
			m_editButton.SetLabel("Edit");
		
		if (newMode == PQD_EditorMode.ENTITY)
		{
			if (!editedEntity)
			{
				OnCriticalError(PQD_ProcessingStep.DEFAULT, "SetLoadoutEditorMode: null entity");
				return;
			}
			m_EditedEntity = editedEntity;
		}
		else
		{
			m_EditedEntity = m_CharacterEntity;
			if (m_wPreviewWidgetComponent)
				m_wPreviewWidgetComponent.SetPreviewedEntity(PQD_EditorMode.CHARACTER, m_CharacterEntity, null);
		}

		switch (newMode)
		{
			case PQD_EditorMode.STORAGE:
				// Show arsenal items panel in STORAGE mode
				if (m_InventoryPanelWidgetComponent)
				{
					m_InventoryPanelWidgetComponent.ShowPanel();
					ListAllArsenalItemsForPanel();
				}
				break;
			case PQD_EditorMode.OPTIONS:
				break;
			case PQD_EditorMode.LOADOUTS:
			case PQD_EditorMode.SERVER_LOADOUTS:
				SetLoadoutButtonsActive(true);
				break;
		}

		CreateSlotsForCurrentEntity();
		UpdateEditedSlotInfo();
	}
	
	//------------------------------------------------------------------------------------------------
	void OnInventoryPanelItemFocused() {}
	void OnSlotsPanelFocused() {}
	void OnInventoryPanelItemFocusChanged(PQD_SlotsUIComponent component, Managed data)
	{
		// Mark that Available Items panel has focus
		m_bLastFocusWasInventoryPanel = true;
		SetRemoveItemButtonActive(false);
	}
	void OnItemAddRequested(PQD_SlotsUIComponent component, Managed data, int button)
	{
		// Check if this is a category click
		PQD_SlotChoiceCategory categoryChoice = PQD_SlotChoiceCategory.Cast(data);
		if (categoryChoice)
		{
			// Navigate into the category
			Print(string.Format("[PQD] OnItemAddRequested: Category clicked: %1", categoryChoice.category), LogLevel.DEBUG);
			m_InventoryPanelWidgetComponent.EnterCategory(categoryChoice.category);
			ListAllArsenalItemsForPanel();
			return;
		}

		PQD_SlotInfo editedSlot = GetCurrentlyEditedSlot();
		if (!editedSlot)
			return;

		PQD_SlotChoice choiceInfo = PQD_SlotChoice.Cast(data);
		if (!choiceInfo)
			return;

		// Validate rank and supply requirements before proceeding
		if (!ValidateItemRequirements(choiceInfo.prefab))
			return;

		IEntity attachedEntity = editedSlot.slot.GetAttachedEntity();
		if (!attachedEntity)
		{
			OnCriticalError(PQD_ProcessingStep.DEFAULT, "Add item requested but attached entity is null");
			return;
		}

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(attachedEntity.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
		{
			OnCriticalError(PQD_ProcessingStep.DEFAULT, "Add item requested but storage component is null");
			return;
		}

		PQD_StorageRequest request = new PQD_StorageRequest();
		request.arsenalEntityRplId = PQD_Helpers.GetEntityRplId(m_ArsenalEntity);
		request.actionType = PQD_ActionType.ADD_ITEM;
		request.storageRplId = m_Cache.GetStorageRplId(storage);
		request.storageSlotId = -1;
		request.prefab = choiceInfo.prefab;

		m_pcComponent.RequestAction(request);
	}
	void UpdateEditedSlotInfo() {}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForSlot()
	{
		if (m_slotHistory.Count() == 0)
		{
			Print("[PQD] CreateOptionsForSlot: No slot in history", LogLevel.WARNING);
			return;
		}
		
		PQD_SlotInfo slotInfo = m_slotHistory.Get(m_slotHistory.Count() - 1);
		if (!slotInfo)
		{
			Print("[PQD] CreateOptionsForSlot: NULL slot info", LogLevel.WARNING);
			return;
		}
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
		
		// Handle storage mode
		if (m_eCurrentMode == PQD_EditorMode.STORAGE)
		{
			ListItemsForStorage();
			return;
		}
		
		// Handle options mode
		if (m_eCurrentMode == PQD_EditorMode.OPTIONS)
		{
			// In options mode, slotInfo should be PQD_EditorOptionData
			PQD_EditorOptionData optionData = PQD_EditorOptionData.Cast(m_wSlotChoicesListbox.GetItemData(m_wSlotChoicesListbox.GetFocusedItem()));
			CreateSlotsForEditorOptions(optionData);
			return;
		}
		
		// Get choices from cache
		array<ResourceName> prefabChoices = {};
		int numChoices = m_Cache.GetChoicesForSlotType(slotInfo, prefabChoices);
		
		Print(string.Format("[PQD] CreateOptionsForSlot: Found %1 choices for slot type %2", numChoices, slotInfo.slotType), LogLevel.DEBUG);
		
		// Create options based on slot type
		switch (slotInfo.slotType)
		{
			case PQD_SlotType.CHARACTER_LOADOUT:
				CreateOptionsForCharacterLoadoutSlot(slotInfo, prefabChoices);
				break;
			case PQD_SlotType.CHARACTER_WEAPON:
				CreateOptionsForCharacterWeaponSlot(slotInfo, prefabChoices);
				break;
			case PQD_SlotType.ATTACHMENT:
				CreateOptionsForAttachmentSlot(slotInfo, prefabChoices);
				break;
			case PQD_SlotType.CHARACTER_EQUIPMENT:
				CreateOptionsForEquipmentSlot(slotInfo, prefabChoices);
				break;
			case PQD_SlotType.MAGAZINE:
				CreateOptionsForMagazineSlot(slotInfo, prefabChoices);
				break;
		}
		
		if (numChoices == 0)
			CreateDummy("No items found for slot");
		
		if (m_wSlotChoicesListbox && m_wSlotChoicesListbox.GetFocusedItem() == -1)
			m_wSlotChoicesListbox.SetFocus(0);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateRemoveSlotOption(PQD_SlotInfo slotInfo)
	{
		if (!slotInfo.slot.GetAttachedEntity())
			return;
		
		PQD_SlotChoice removeChoice = new PQD_SlotChoice();
		removeChoice.slotType = PQD_SlotType.OPTION;
		removeChoice.slotName = "[ Remove Item ]";
		removeChoice.prefab = "";
		removeChoice.slotEnabled = true;
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.AddItem_Choice(removeChoice);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForCharacterLoadoutSlot(PQD_SlotInfo slotInfo, array<ResourceName> prefabChoices)
	{
		CreateRemoveSlotOption(slotInfo);
		
		ResourceName attachedItemResourceName;
		PQD_Helpers.GetResourceNameFromEntity(slotInfo.slot.GetAttachedEntity(), attachedItemResourceName);
		
		int listBoxChildId;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		BaseInventoryStorageComponent slotStorage = slotInfo.slot.GetStorage();
		SCR_CharacterInventoryStorageComponent characterStorage = SCR_CharacterInventoryStorageComponent.Cast(slotStorage);
		
		foreach (ResourceName resourceName : prefabChoices)
		{
			Resource loadedResource = Resource.Load(resourceName);
			itemEntity = GetGame().SpawnEntityPrefabLocal(loadedResource, world, null);
			if (!itemEntity)
				continue;
			
			PQD_SlotChoice optionSlotInfo = new PQD_SlotChoice();
			optionSlotInfo.prefab = resourceName;
			optionSlotInfo.slotEnabled = true;
			
			if (!PQD_Helpers.GetItemNameFromEntityValidate(itemEntity, optionSlotInfo.slotName))
			{
				Print(string.Format("[PQD] Prefab %1 is invalid!", resourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			// Check if can replace
			if (!slotStorage.CanReplaceItem(itemEntity, slotInfo.storageSlotId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			// Check blocked slots
			if (characterStorage)
			{
				BaseLoadoutClothComponent itemClothComponent = BaseLoadoutClothComponent.Cast(itemEntity.FindComponent(BaseLoadoutClothComponent));
				if (itemClothComponent)
				{
					array<typename> blockedSlots = {};
					int blockedSlotsCount = itemClothComponent.GetBlockedSlots(blockedSlots);
					if (blockedSlotsCount > 0)
					{
						foreach (typename blockedAreaType : blockedSlots)
						{
							if (characterStorage.GetClothFromArea(blockedAreaType) || characterStorage.IsAreaBlocked(blockedAreaType))
							{
								optionSlotInfo.slotEnabled = false;
								optionSlotInfo.itemName = "(blocked)";
								break;
							}
						}
					}
				}
			}
			
			listBoxChildId = m_wSlotChoicesListbox.AddItem_Choice(optionSlotInfo);
			if (attachedItemResourceName == resourceName)
				m_wSlotChoicesListbox.SetFocus(listBoxChildId);
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForCharacterWeaponSlot(PQD_SlotInfo slotInfo, array<ResourceName> prefabChoices)
	{
		CreateRemoveSlotOption(slotInfo);
		
		ResourceName attachedItemResourceName;
		PQD_Helpers.GetResourceNameFromEntity(slotInfo.slot.GetAttachedEntity(), attachedItemResourceName);
		
		int listBoxChildId;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (ResourceName resourceName : prefabChoices)
		{
			Resource loadedResource = Resource.Load(resourceName);
			itemEntity = GetGame().SpawnEntityPrefabLocal(loadedResource, world, null);
			if (!itemEntity)
				continue;
			
			PQD_SlotChoice optionSlotInfo = new PQD_SlotChoice();
			optionSlotInfo.prefab = resourceName;
			
			if (!PQD_Helpers.GetItemNameFromEntityValidate(itemEntity, optionSlotInfo.slotName))
			{
				Print(string.Format("[PQD] Prefab %1 is invalid!", resourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			listBoxChildId = m_wSlotChoicesListbox.AddItem_Choice(optionSlotInfo);
			if (attachedItemResourceName == resourceName)
				m_wSlotChoicesListbox.SetFocus(listBoxChildId);
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForAttachmentSlot(PQD_SlotInfo slotInfo, array<ResourceName> prefabChoices)
	{
		CreateRemoveSlotOption(slotInfo);
		
		ResourceName attachedItemResourceName;
		PQD_Helpers.GetResourceNameFromEntity(slotInfo.slot.GetAttachedEntity(), attachedItemResourceName);
		
		int listBoxChildId;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (ResourceName resourceName : prefabChoices)
		{
			Resource loadedResource = Resource.Load(resourceName);
			itemEntity = GetGame().SpawnEntityPrefabLocal(loadedResource, world, null);
			if (!itemEntity)
				continue;
			
			PQD_SlotChoice optionSlotInfo = new PQD_SlotChoice();
			optionSlotInfo.prefab = resourceName;
			
			if (!PQD_Helpers.GetItemNameFromEntityValidate(itemEntity, optionSlotInfo.slotName))
			{
				Print(string.Format("[PQD] Prefab %1 is invalid!", resourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			listBoxChildId = m_wSlotChoicesListbox.AddItem_Choice(optionSlotInfo);
			if (attachedItemResourceName == resourceName)
				m_wSlotChoicesListbox.SetFocus(listBoxChildId);
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForEquipmentSlot(PQD_SlotInfo slotInfo, array<ResourceName> prefabChoices)
	{
		CreateRemoveSlotOption(slotInfo);
		
		ResourceName attachedItemResourceName;
		PQD_Helpers.GetResourceNameFromEntity(slotInfo.slot.GetAttachedEntity(), attachedItemResourceName);
		
		int listBoxChildId;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (ResourceName resourceName : prefabChoices)
		{
			Resource loadedResource = Resource.Load(resourceName);
			itemEntity = GetGame().SpawnEntityPrefabLocal(loadedResource, world, null);
			if (!itemEntity)
				continue;
			
			PQD_SlotChoice optionSlotInfo = new PQD_SlotChoice();
			optionSlotInfo.prefab = resourceName;
			
			if (!PQD_Helpers.GetItemNameFromEntityValidate(itemEntity, optionSlotInfo.slotName))
			{
				Print(string.Format("[PQD] Prefab %1 is invalid!", resourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			listBoxChildId = m_wSlotChoicesListbox.AddItem_Choice(optionSlotInfo);
			if (attachedItemResourceName == resourceName)
				m_wSlotChoicesListbox.SetFocus(listBoxChildId);
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateOptionsForMagazineSlot(PQD_SlotInfo slotInfo, array<ResourceName> prefabChoices)
	{
		CreateRemoveSlotOption(slotInfo);
		
		ResourceName attachedItemResourceName;
		PQD_Helpers.GetResourceNameFromEntity(slotInfo.slot.GetAttachedEntity(), attachedItemResourceName);
		
		int listBoxChildId;
		IEntity itemEntity;
		BaseWorld world = GetGame().GetWorld();
		
		foreach (ResourceName resourceName : prefabChoices)
		{
			Resource loadedResource = Resource.Load(resourceName);
			itemEntity = GetGame().SpawnEntityPrefabLocal(loadedResource, world, null);
			if (!itemEntity)
				continue;
			
			PQD_SlotChoice optionSlotInfo = new PQD_SlotChoice();
			optionSlotInfo.prefab = resourceName;
			
			if (!PQD_Helpers.GetItemNameFromEntityValidate(itemEntity, optionSlotInfo.slotName))
			{
				Print(string.Format("[PQD] Prefab %1 is invalid!", resourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
				continue;
			}
			
			listBoxChildId = m_wSlotChoicesListbox.AddItem_Choice(optionSlotInfo);
			if (attachedItemResourceName == resourceName)
				m_wSlotChoicesListbox.SetFocus(listBoxChildId);
			
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void ListItemsForStorage()
	{
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
		
		PQD_SlotInfo currentSlot = GetCurrentlyEditedSlot();
		if (!currentSlot)
		{
			Print("[PQD] ListItemsForStorage: No current slot in history", LogLevel.WARNING);
			CreateDummy("Select a storage container first");
			return;
		}
		
		IEntity attachedEntity = currentSlot.slot.GetAttachedEntity();
		if (!attachedEntity)
		{
			Print("[PQD] ListItemsForStorage: No attached entity in slot", LogLevel.WARNING);
			CreateDummy("This storage is empty");
			return;
		}
		
		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(attachedEntity.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
		{
			Print("[PQD] ListItemsForStorage: No storage component found", LogLevel.WARNING);
			CreateDummy("This item has no storage");
			return;
		}
		
		array<IEntity> items = {};
		storage.GetAll(items);
		
		Print(string.Format("[PQD] ListItemsForStorage: Found %1 items in storage", items.Count()), LogLevel.DEBUG);
		
		if (items.Count() == 0)
		{
			CreateDummy("Storage is empty");
			return;
		}
		
		foreach (IEntity item : items)
		{
			if (!item)
				continue;
			
			InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!itemComp)
				continue;
			
			InventoryStorageSlot parentSlot = itemComp.GetParentSlot();
			if (!parentSlot)
				continue;
			
			PQD_SlotInfoStorageItem itemInfo = new PQD_SlotInfoStorageItem();
			itemInfo.slotType = PQD_SlotType.INVENTORY;
			itemInfo.storageType = PQD_StorageType.INVENTORY;
			itemInfo.slot = parentSlot;
			itemInfo.storageSlotId = parentSlot.GetID();
			itemInfo.slotName = PQD_Helpers.GetItemNameFromEntity(item);
			itemInfo.itemName = "";  // Don't duplicate the name
			itemInfo.slotEnabled = true;
			
			// Get prefab for preview
			ResourceName prefab;
			if (PQD_Helpers.GetResourceNameFromEntity(item, prefab))
				itemInfo.prefab = prefab;
			
			if (m_wSlotChoicesListbox)
				m_wSlotChoicesListbox.AddItem_Slot(itemInfo);
		}
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.SetFocus(0);
	}
	
	//------------------------------------------------------------------------------------------------
	// List ALL arsenal items for the inventory panel (without needing a slot selected)
	void ListAllArsenalItemsForPanel()
	{
		bool inSubMenu = false;
		if (m_InventoryPanelWidgetComponent)
			inSubMenu = m_InventoryPanelWidgetComponent.IsInSubMenu();

		Print(string.Format("[PQD] ListAllArsenalItemsForPanel: Starting, IsInSubMenu=%1", inSubMenu), LogLevel.NORMAL);

		if (!m_InventoryPanelWidgetComponent)
		{
			Print("[PQD] ListAllArsenalItemsForPanel: m_InventoryPanelWidgetComponent is null!", LogLevel.WARNING);
			return;
		}

		// Clear existing items
		m_InventoryPanelWidgetComponent.Clear();

		// Check if we're in a submenu (viewing a specific category)
		if (m_InventoryPanelWidgetComponent.IsInSubMenu())
		{
			PQD_ItemCategory currentCat = m_InventoryPanelWidgetComponent.GetCurrentCategory();
			Print(string.Format("[PQD] ListAllArsenalItemsForPanel: In submenu, category=%1", currentCat), LogLevel.NORMAL);
			ListArsenalItemsByCategory(currentCat);
			return;
		}

		// Show main category selection menu
		Print("[PQD] ListAllArsenalItemsForPanel: Showing category menu", LogLevel.NORMAL);

		// Count items in each category to decide whether to show it
		// Use separate arrays to avoid issues with cache
		array<SCR_ArsenalItem> ammoItems = {};
		SCR_EArsenalItemType ammoTypes = SCR_EArsenalItemType.PISTOL | SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.MACHINE_GUN;
		int numAmmo = m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.AMMUNITION, ammoTypes, ammoItems);

		array<SCR_ArsenalItem> equipmentItems = {};
		int numEquipment = m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.DEFAULT, SCR_EArsenalItemType.EQUIPMENT, equipmentItems);

		array<SCR_ArsenalItem> healItems = {};
		int numHeal = m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.CONSUMABLE, SCR_EArsenalItemType.HEAL, healItems);

		// Create category options
		if (numAmmo > 0)
		{
			PQD_SlotChoiceCategory ammoCategory = new PQD_SlotChoiceCategory();
			ammoCategory.category = PQD_ItemCategory.AMMUNITION;
			ammoCategory.slotName = string.Format("Ammunition (%1 items)", numAmmo);
			ammoCategory.slotEnabled = true;
			ammoCategory.slotType = PQD_SlotType.ARSENAL_ITEM;
			// Use first item's prefab as icon
			if (ammoItems.Count() > 0)
				ammoCategory.prefab = ammoItems[0].GetItemResourceName();
			m_InventoryPanelWidgetComponent.AddCategoryChoice(ammoCategory);
		}

		if (numEquipment > 0)
		{
			PQD_SlotChoiceCategory equipmentCategory = new PQD_SlotChoiceCategory();
			equipmentCategory.category = PQD_ItemCategory.EQUIPMENT;
			equipmentCategory.slotName = string.Format("Equipment (%1 items)", numEquipment);
			equipmentCategory.slotEnabled = true;
			equipmentCategory.slotType = PQD_SlotType.ARSENAL_ITEM;
			// Use first item's prefab as icon
			if (equipmentItems.Count() > 0)
				equipmentCategory.prefab = equipmentItems[0].GetItemResourceName();
			m_InventoryPanelWidgetComponent.AddCategoryChoice(equipmentCategory);
		}

		if (numHeal > 0)
		{
			PQD_SlotChoiceCategory medicalCategory = new PQD_SlotChoiceCategory();
			medicalCategory.category = PQD_ItemCategory.MEDICAL;
			medicalCategory.slotName = string.Format("Medical (%1 items)", numHeal);
			medicalCategory.slotEnabled = true;
			medicalCategory.slotType = PQD_SlotType.ARSENAL_ITEM;
			// Use first item's prefab as icon
			if (healItems.Count() > 0)
				medicalCategory.prefab = healItems[0].GetItemResourceName();
			m_InventoryPanelWidgetComponent.AddCategoryChoice(medicalCategory);
		}

		int totalItems = numAmmo + numEquipment + numHeal;
		if (totalItems == 0)
		{
			Print("[PQD] ListAllArsenalItemsForPanel: No items found - check if arsenal has items configured", LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	// List items for a specific category
	void ListArsenalItemsByCategory(PQD_ItemCategory category)
	{
		Print(string.Format("[PQD] ListArsenalItemsByCategory: Category %1", category), LogLevel.NORMAL);

		if (!m_InventoryPanelWidgetComponent)
		{
			Print("[PQD] ListArsenalItemsByCategory: m_InventoryPanelWidgetComponent is null!", LogLevel.WARNING);
			return;
		}

		m_InventoryPanelWidgetComponent.Clear();

		array<SCR_ArsenalItem> items = {};
		string categoryName;

		switch (category)
		{
			case PQD_ItemCategory.AMMUNITION:
			{
				categoryName = "AMMUNITION";
				SCR_EArsenalItemType ammoTypes = SCR_EArsenalItemType.PISTOL | SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.MACHINE_GUN;
				m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.AMMUNITION, ammoTypes, items);
				break;
			}
			case PQD_ItemCategory.EQUIPMENT:
			{
				categoryName = "EQUIPMENT";
				m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.DEFAULT, SCR_EArsenalItemType.EQUIPMENT, items);
				break;
			}
			case PQD_ItemCategory.MEDICAL:
			{
				categoryName = "MEDICAL";
				m_Cache.GetArsenalItemsByMode(SCR_EArsenalItemMode.CONSUMABLE, SCR_EArsenalItemType.HEAL, items);
				break;
			}
		}

		Print(string.Format("[PQD] ListArsenalItemsByCategory: Found %1 items for category %2", items.Count(), categoryName), LogLevel.NORMAL);

		// Add category header with item count
		string headerText = string.Format("--- %1: %2 items ---", categoryName, items.Count());
		m_InventoryPanelWidgetComponent.AddCategoryHeader(headerText);

		// Add all items in this category
		foreach (SCR_ArsenalItem arsenalItem : items)
		{
			m_InventoryPanelWidgetComponent.AddArsenalItem(arsenalItem);
		}

		Print(string.Format("[PQD] ListArsenalItemsByCategory: Added %1 items successfully", items.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void HandleChangedIdentity(PQD_SlotChoiceIdentity slotChoice) {}
	
	//------------------------------------------------------------------------------------------------
	void HandleChangedOption(PQD_SlotChoice slotChoice)
	{
		if (!slotChoice)
		{
			PQD_Helpers.PlaySound("blocked");
			SetUIWaiting(false);
			return;
		}
		
		PQD_SlotInfo editedSlot = GetCurrentlyEditedSlot();
		if (!editedSlot)
		{
			SetUIWaiting(false);
			return;
		}
		
		// Handle remove item option
		if (slotChoice.prefab.IsEmpty())
		{
			// Request remove
			PQD_StorageRequest request = new PQD_StorageRequest();
			request.arsenalEntityRplId = PQD_Helpers.GetEntityRplId(m_ArsenalEntity);
			request.actionType = PQD_ActionType.REMOVE_ITEM;
			request.storageRplId = m_Cache.GetStorageRplId(editedSlot.slot.GetStorage());
			request.storageSlotId = editedSlot.storageSlotId;
			
			SetUIWaiting(true);
			m_pcComponent.RequestAction(request);
			return;
		}
		
		// Validate rank and supply requirements before proceeding
		if (!ValidateItemRequirements(slotChoice.prefab))
		{
			SetUIWaiting(false);
			return;
		}
		
		// Request swap/add item
		PQD_StorageRequest request = new PQD_StorageRequest();
		request.arsenalEntityRplId = PQD_Helpers.GetEntityRplId(m_ArsenalEntity);
		request.actionType = PQD_ActionType.REPLACE_ITEM;
		request.storageRplId = m_Cache.GetStorageRplId(editedSlot.slot.GetStorage());
		request.storageSlotId = editedSlot.storageSlotId;
		request.prefab = slotChoice.prefab;
		
		if (m_wSlotChoicesListbox)
			m_iListBoxLastActionChild = m_wSlotChoicesListbox.GetFocusedItem();
		
		SetUIWaiting(true);
		m_pcComponent.RequestAction(request);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Validate that the player meets rank and supply requirements for an item
	//! Returns true if the item can be equipped, false otherwise (and shows error message)
	bool ValidateItemRequirements(ResourceName prefab)
	{
		if (!m_Cache || prefab.IsEmpty())
			return true;
		
		float cost;
		SCR_ECharacterRank requiredRank;
		m_Cache.GetArsenalItemCostAndRank(prefab, cost, requiredRank);
		
		// Check rank requirement
		if (requiredRank != SCR_ECharacterRank.INVALID && requiredRank != SCR_ECharacterRank.RENEGADE)
		{
			SCR_ECharacterRank playerRank = SCR_CharacterRankComponent.GetCharacterRank(m_CharacterEntity);
			
			// Compare rank levels (higher enum value = higher rank)
			if (playerRank < requiredRank)
			{
				string requiredRankStr = typename.EnumToString(SCR_ECharacterRank, requiredRank);
				string playerRankStr = typename.EnumToString(SCR_ECharacterRank, playerRank);
				
				if (m_wStatusMessageWidget)
					m_wStatusMessageWidget.ShowMessage(string.Format("Rank required: %1 (You are %2)", requiredRankStr, playerRankStr), PQD_MessageType.ERROR);
				
				PQD_Helpers.PlaySound("blocked");
				return false;
			}
		}
		
		// Check supply requirement
		if (cost > 0 && m_Cache.AreSuppliesEnabled())
		{
			float availableSupply = 0;
			if (m_RankAndSupplyComponent)
				availableSupply = m_RankAndSupplyComponent.GetAvailableSupply();
			
			if (availableSupply < cost)
			{
				if (m_wStatusMessageWidget)
					m_wStatusMessageWidget.ShowMessage(string.Format("Not enough supplies! Need: %1, Have: %2", Math.Round(cost), Math.Round(availableSupply)), PQD_MessageType.ERROR);
				
				PQD_Helpers.PlaySound("blocked");
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleChangedLoadoutOption(PQD_SlotLoadout slotInfo, int button)
	{
		if (!slotInfo)
		{
			PQD_Helpers.PlaySound("blocked");
			SetUIWaiting(false);
			return;
		}
		
		SetUIWaiting(true);
		
		PQD_LoadoutRequest request = new PQD_LoadoutRequest();
		request.arsenalComponentRplId = m_ArsenalComponentRplId;
		
		if (m_wSlotChoicesListbox)
			m_iListBoxLastActionChild = m_wSlotChoicesListbox.GetFocusedItem();

		switch (button)
		{
			// Left click - Apply loadout
			case 0:
				if (slotInfo.metadataWeapons.IsEmpty())
				{
					PQD_Helpers.PlaySound("blocked");
					SetUIWaiting(false);
					return;
				}
				request.actionType = PQD_ActionType.APPLY_LOADOUT;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
			
			// Right click / Save button - Save loadout
			case 1:
				request.actionType = PQD_ActionType.SAVE_LOADOUT;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
			
			// Admin Apply
			case 2:
				if (slotInfo.metadataWeapons.IsEmpty())
				{
					PQD_Helpers.PlaySound("blocked");
					SetUIWaiting(false);
					return;
				}
				request.actionType = PQD_ActionType.APPLY_LOADOUT_ADMIN;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
			
			// Admin Save
			case 3:
				request.actionType = PQD_ActionType.SAVE_LOADOUT_ADMIN;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
			
			// Clear loadout
			case 100:
				request.actionType = PQD_ActionType.CLEAR_LOADOUT;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
			
			// Admin Clear loadout
			case 101:
				request.actionType = PQD_ActionType.CLEAR_LOADOUT_ADMIN;
				request.loadoutSlotId = slotInfo.loadoutSlotId;
				break;
		}

		m_pcComponent.RequestLoadoutAction(request);
	}

	//------------------------------------------------------------------------------------------------
	void CreateSlotsForCurrentEntity()
	{
		if (!m_EditedEntity)
		{
			OnCriticalError(PQD_ProcessingStep.DEFAULT, "CreateSlotsForCurrentEntity: null entity");
			return;
		}
	
		switch (m_eCurrentMode)
		{
			case PQD_EditorMode.CHARACTER:
				CreateSlotsForMultipleStorages();
				break;
			case PQD_EditorMode.ENTITY:
				CreateSlotsForStorage(PQD_StorageType.WEAPON, 
					BaseInventoryStorageComponent.Cast(m_EditedEntity.FindComponent(BaseInventoryStorageComponent)));
				break;
			case PQD_EditorMode.STORAGE:
				CreateSlotsForCharacterStorages();
				break;
			case PQD_EditorMode.OPTIONS:
				CreateSlotsForEditorOptions();
				break;
			case PQD_EditorMode.LOADOUTS:
				RequestLoadoutSlotOptions();
				return;
			case PQD_EditorMode.SERVER_LOADOUTS:
				RequestLoadoutSlotOptions(true);
				return;
		}
		
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSlotsForMultipleStorages()
	{
		array<BaseInventoryStorageComponent> storages = {};
		int numStorages = PQD_Helpers.GetAllEntityStorages(m_EditedEntity, storages);
		
		if (numStorages < 1)
		{
			ShowWarning("No storage component found in entity");
			return;
		}
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
		
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			PQD_StorageType storageType = m_Cache.GetStorageType(storage);
			if (storageType == PQD_StorageType.IGNORED)
				continue;
			
			if (m_SlotCategoryWidgetComponent && !m_SlotCategoryWidgetComponent.ShouldViewCharacterStorageCategory(storageType))
				continue;
			
			CreateSlotsForStorage(storageType, storage, false);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSlotsForCharacterStorages()
	{
		array<InventoryStorageSlot> storageSlots = {};
		int numStorageSlots = PQD_Helpers.GetAllCharacterItemStorageSlots(m_CharacterEntity, storageSlots);
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
		
		if (numStorageSlots < 1)
		{
			CreateDummy("No storage containers found");
			return;
		}

		Print(string.Format("[PQD] CreateSlotsForCharacterStorages: Found %1 storage containers", numStorageSlots), LogLevel.DEBUG);

		foreach (InventoryStorageSlot storageSlot : storageSlots)
		{
			IEntity containerEntity = storageSlot.GetAttachedEntity();
			if (!containerEntity)
				continue;
			
			// Create slot info for each container (backpack, vest, etc.)
			PQD_SlotInfo slotInfo = new PQD_SlotInfo();
			slotInfo.slot = storageSlot;
			slotInfo.storageSlotId = storageSlot.GetID();
			slotInfo.storageType = PQD_StorageType.CHARACTER_LOADOUT;
			slotInfo.slotType = PQD_SlotType.CHARACTER_LOADOUT;
			slotInfo.slotEnabled = true;
			
			// Get container name (e.g., "Backpack", "Vest")
			slotInfo.slotName = storageSlot.GetSourceName();
			if (slotInfo.slotName.IsEmpty())
				slotInfo.slotName = "Container";
			
			// Get item name (e.g., "ALICE Pack", "Tactical Vest")
			slotInfo.itemName = PQD_Helpers.GetItemNameFromEntity(containerEntity);
			
			// Container always has storage (that's why it's in this list)
			slotInfo.hasStorage = true;
			
			// Get prefab for preview
			ResourceName prefab;
			if (PQD_Helpers.GetResourceNameFromEntity(containerEntity, prefab))
				slotInfo.prefab = prefab;
			
			if (m_wSlotChoicesListbox)
				slotInfo.listBoxChildId = m_wSlotChoicesListbox.AddItem_Slot(slotInfo);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSlotsForStorage(PQD_StorageType storageType, BaseInventoryStorageComponent storage, bool clearFirst = true)
	{
		if (!storage)
		{
			Print("[PQD] CreateSlotsForStorage: storage is null", LogLevel.WARNING);
			return;
		}
		
		if (clearFirst && m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();

		int count = storage.GetSlotsCount();
		
		// Collect slot infos for hotspot creation
		array<ref PQD_SlotInfo> slotInfos = {};

		for (int i = 0; i < count; ++i)
		{
			PQD_SlotInfo slotInfo = CreateStorageSlotWithInfo(storageType, storage, i);
			if (slotInfo)
				slotInfos.Insert(slotInfo);
		}
		
		// Create hotspots on the 3D preview for ENTITY mode (weapons)
		if (m_eCurrentMode == PQD_EditorMode.ENTITY && m_wPreviewWidgetComponent && slotInfos.Count() > 0)
		{
			m_wPreviewWidgetComponent.CreateSlotHotspots(slotInfos);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	PQD_SlotInfo CreateStorageSlotWithInfo(PQD_StorageType storageType, BaseInventoryStorageComponent storage, int storageSlotId)
	{
		InventoryStorageSlot storageSlot = storage.GetSlot(storageSlotId);

		PQD_SlotInfo slotInfo = new PQD_SlotInfo();
		if (!PQD_Helpers.ValidateAndFillSlotInfo(storageSlot, slotInfo))
		{
			slotInfo.slotEnabled = false;
		}
		else
		{
			IEntity ent = storageSlot.GetAttachedEntity();
			
			// Check for editable storage - for STORAGE mode (Items tab), containers are editable
			if (m_eCurrentMode == PQD_EditorMode.CHARACTER || m_eCurrentMode == PQD_EditorMode.ENTITY)
				slotInfo.hasStorage = PQD_Helpers.IsItemInSlotEditable(ent);
			else if (m_eCurrentMode == PQD_EditorMode.STORAGE)
			{
				// In Storage mode, containers (items with storage) can be opened
				if (ent)
				{
					BaseInventoryStorageComponent itemStorage = BaseInventoryStorageComponent.Cast(ent.FindComponent(BaseInventoryStorageComponent));
					slotInfo.hasStorage = (itemStorage != null);
				}
			}

			slotInfo.itemName = PQD_Helpers.GetItemNameFromEntity(ent);
			
			// Get prefab for 3D preview
			ResourceName itemPrefab;
			if (PQD_Helpers.GetResourceNameFromEntity(ent, itemPrefab))
				slotInfo.prefab = itemPrefab;
			
			Print(string.Format("[PQD] CreateStorageSlot: slot=%1, hasStorage=%2, mode=%3, ent=%4", 
				slotInfo.slotName, slotInfo.hasStorage, m_eCurrentMode, ent), LogLevel.NORMAL);
		}

		slotInfo.storageType = storageType;
		slotInfo.slot = storageSlot;

		if (m_wSlotChoicesListbox)
			slotInfo.listBoxChildId = m_wSlotChoicesListbox.AddItem_Slot(slotInfo);
		
		return slotInfo;
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSlotsForEditorOptions(PQD_EditorOptionData optionData = null)
	{
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();
		
		// If we have a selected option, show its choices
		if (optionData)
		{
			PQD_EditorOptionComponent optionComponent = m_EditorOptions.Get(optionData.editorOptionLabel);
			if (optionComponent)
			{
				array<PQD_EditorOptionData> optionChoices = {};
				int choices = optionComponent.GetChoices(optionChoices);
				
				for (int i = 0; i < choices; i++)
				{
					m_wSlotChoicesListbox.AddItem_Option(optionChoices[i]);
				}
			}
			return;
		}
		
		// Show all registered options with their current values
		if (m_EditorOptions.Count() > 0)
		{
			foreach (string key, PQD_EditorOptionComponent optionComponent : m_EditorOptions)
			{
				PQD_EditorOptionData selected = optionComponent.GetSelectedOption();
				if (selected)
					m_wSlotChoicesListbox.AddItem_Option(selected);
			}
		}
		else
		{
			// No options registered - show default info
			CreateDefaultEditorOptions();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateDefaultEditorOptions()
	{
		// These are placeholder options - can be expanded later
		// For now just show info about keybinds
		PQD_EditorOptionData keybindsInfo = new PQD_EditorOptionData();
		keybindsInfo.editorOptionLabel = "Controls";
		keybindsInfo.optionLabel = "Q/E: Change tabs | F: Edit item | ESC: Back";
		keybindsInfo.optionEnabled = false;
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.AddItem_Option(keybindsInfo);
		
		PQD_EditorOptionData versionInfo = new PQD_EditorOptionData();
		versionInfo.editorOptionLabel = "Version";
		versionInfo.optionLabel = "PQD Loadout Editor v1.0.0";
		versionInfo.optionEnabled = false;
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.AddItem_Option(versionInfo);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateStorageSlot(PQD_StorageType storageType, BaseInventoryStorageComponent storage, int storageSlotId)
	{
		InventoryStorageSlot storageSlot = storage.GetSlot(storageSlotId);

		PQD_SlotInfo slotInfo = new PQD_SlotInfo();
		if (!PQD_Helpers.ValidateAndFillSlotInfo(storageSlot, slotInfo))
		{
			slotInfo.slotEnabled = false;
		}
		else
		{
			IEntity ent = storageSlot.GetAttachedEntity();
			
			// Check for editable storage - for STORAGE mode (Items tab), containers are editable
			if (m_eCurrentMode == PQD_EditorMode.CHARACTER || m_eCurrentMode == PQD_EditorMode.ENTITY)
				slotInfo.hasStorage = PQD_Helpers.IsItemInSlotEditable(ent);
			else if (m_eCurrentMode == PQD_EditorMode.STORAGE)
			{
				// In Storage mode, containers (items with storage) can be opened
				if (ent)
				{
					BaseInventoryStorageComponent itemStorage = BaseInventoryStorageComponent.Cast(ent.FindComponent(BaseInventoryStorageComponent));
					slotInfo.hasStorage = (itemStorage != null);
				}
			}

			slotInfo.itemName = PQD_Helpers.GetItemNameFromEntity(ent);
			
			// Get prefab for 3D preview
			ResourceName itemPrefab;
			if (PQD_Helpers.GetResourceNameFromEntity(ent, itemPrefab))
				slotInfo.prefab = itemPrefab;
			
			Print(string.Format("[PQD] CreateStorageSlot: slot=%1, hasStorage=%2, mode=%3, ent=%4", 
				slotInfo.slotName, slotInfo.hasStorage, m_eCurrentMode, ent), LogLevel.NORMAL);
		}

		slotInfo.storageType = storageType;
		slotInfo.slot = storageSlot;

		if (m_wSlotChoicesListbox)
			slotInfo.listBoxChildId = m_wSlotChoicesListbox.AddItem_Slot(slotInfo);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateDummy(string message)
	{
		if (!m_wSlotChoicesListbox)
			return;
		
		// Create a disabled slot info to display the message
		PQD_SlotInfo dummyInfo = new PQD_SlotInfo();
		dummyInfo.slotName = message;
		dummyInfo.itemName = "";
		dummyInfo.slotEnabled = false;
		dummyInfo.hasStorage = false;
		
		m_wSlotChoicesListbox.AddItem_Slot(dummyInfo);
	}
	
	//------------------------------------------------------------------------------------------------
	void RequestLoadoutSlotOptions(bool adminLoadouts = false)
	{
		SetUIWaiting(true);
		
		PQD_LoadoutRequest request = new PQD_LoadoutRequest();
		request.arsenalComponentRplId = m_ArsenalComponentRplId;
		
		if (adminLoadouts)
			request.actionType = PQD_ActionType.GET_ADMIN_LOADOUTS;
		else
			request.actionType = PQD_ActionType.GET_LOADOUTS;
		
		m_pcComponent.RequestLoadoutAction(request);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnServerResponse_Storage(PQD_NetworkResponse response, PQD_StorageRequest request)
	{
		HandleMessage(response.success, response.message);
		GetGame().GetCallqueue().CallLater(RefreshUpdatedSlot, 66, false, response.success, request.storageRplId, request.storageSlotId);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnServerResponse_Loadout(PQD_NetworkResponse response, PQD_LoadoutRequest request)
	{
		HandleMessage(response.success, response.message);
		
		if (response.success && (request.actionType == PQD_ActionType.GET_LOADOUTS || 
			request.actionType == PQD_ActionType.GET_ADMIN_LOADOUTS ||
			request.actionType == PQD_ActionType.SAVE_LOADOUT ||
			request.actionType == PQD_ActionType.SAVE_LOADOUT_ADMIN ||
			request.actionType == PQD_ActionType.CLEAR_LOADOUT ||
			request.actionType == PQD_ActionType.CLEAR_LOADOUT_ADMIN))
		{
			CreateSlotsForLoadoutOptions(response.message);
		}
		
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void HandleMessage(bool success, string message)
	{
		if (m_wStatusMessageWidget)
		{
			if (success)
				m_wStatusMessageWidget.ShowMessage(message, PQD_MessageType.OK);
			else
				m_wStatusMessageWidget.ShowMessage(message, PQD_MessageType.ERROR);
		}
		
		if (success)
			PQD_Helpers.PlaySoundForSlotType(PQD_SlotType.OPTION);
		else
			PQD_Helpers.PlaySound("blocked");
	}
	
	//------------------------------------------------------------------------------------------------
	void RefreshUpdatedSlot(bool success, RplId storageRplId, int slotId)
	{
		if (!success)
		{
			SetUIWaiting(false);
			return;
		}
		
		// In STORAGE mode (Items tab), just refresh the inventory list
		if (m_eCurrentMode == PQD_EditorMode.STORAGE)
		{
			// Refresh the items in the current storage
			if (m_slotHistory.Count() > 0)
			{
				ListItemsForStorage();
			}
			else
			{
				// No container selected, refresh the container list
				CreateSlotsForCurrentEntity();
			}
			SetUIWaiting(false);
			return;
		}
		
		// Don't go back - stay in the current view
		// Just refresh the options for the current slot so the user can continue selecting
		PQD_SlotInfo currentSlot = GetCurrentlyEditedSlot();
		if (currentSlot && m_slotHistory.Count() > 0)
		{
			// We're viewing options for a slot - refresh the options list
			CreateOptionsForSlot();
			
			// Restore focus to the previously selected item if possible
			if (m_wSlotChoicesListbox && m_iListBoxLastActionChild >= 0)
			{
				int itemCount = m_wSlotChoicesListbox.GetItemCount();
				if (m_iListBoxLastActionChild < itemCount)
					m_wSlotChoicesListbox.SetFocusedItem(m_iListBoxLastActionChild);
				else if (itemCount > 0)
					m_wSlotChoicesListbox.SetFocusedItem(itemCount - 1);
			}
		}
		else
		{
			// We're at the main level - refresh the entity slots
			CreateSlotsForCurrentEntity();
		}
		
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSlotsForLoadoutOptions(string payload)
	{
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.Clear();

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		ctx.ImportFromString(payload);
		array<ref PQD_PlayerLoadout> loadouts = {};

		if (!ctx.ReadValue("loadouts", loadouts))
		{
			Print("[PQD] Failed to parse loadout options payload", LogLevel.WARNING);
			CreateDummy("Failed to load loadouts");
			SetUIWaiting(false);
			return;
		}

		foreach (PQD_PlayerLoadout loadout : loadouts)
		{
			PQD_SlotLoadout choice = new PQD_SlotLoadout();

			// Use the new HasData() method to check if slot has content
			if (!loadout.metadata_clothes.IsEmpty() && loadout.metadata_clothes != "N/A")
				choice.metadataClothes = loadout.metadata_clothes;
			else if (loadout.metadata_weapons.IsEmpty())
				choice.metadataClothes = "Empty Slot";
			else
				choice.metadataClothes = "Custom Loadout";

			choice.metadataWeapons = loadout.metadata_weapons;
			choice.loadoutSlotId = loadout.slotId;
			choice.supplyCost = loadout.supplyCost;

			// Store character prefab and data for 3D preview with equipped items (only if loadout has data)
			Print(string.Format("[PQD] CreateSlotsForLoadoutOptions: Slot %1, HasData=%2, prefab='%3', data length=%4",
				loadout.slotId, loadout.HasData(), loadout.prefab, loadout.data.Length()), LogLevel.NORMAL);
			if (loadout.HasData() && !loadout.prefab.IsEmpty())
			{
				choice.prefab = loadout.prefab;
				choice.data = loadout.data;
				Print(string.Format("[PQD] CreateSlotsForLoadoutOptions: Assigned prefab and data to choice: '%1'", choice.prefab), LogLevel.NORMAL);
			}

			// Parse required rank enum
			if (!loadout.required_rank.IsEmpty())
				choice.requiredRank = typename.StringToEnum(SCR_ECharacterRank, loadout.required_rank);
			else
				choice.requiredRank = SCR_ECharacterRank.INVALID;

			if (m_wSlotChoicesListbox)
				m_wSlotChoicesListbox.AddItem_Loadout(choice);
		}
		
		if (loadouts.Count() == 0)
		{
			CreateDummy("No loadout slots available");
		}
		
		if (m_wSlotChoicesListbox)
			m_wSlotChoicesListbox.SetFocus(0);
		
		SetUIWaiting(false);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle click on 3D slot hotspot - selects the corresponding slot in the listbox
	void OnSlotHotspotClicked(int listboxIndex)
	{
		Print(string.Format("[PQD] OnSlotHotspotClicked: index=%1", listboxIndex), LogLevel.NORMAL);
		
		if (!m_wSlotChoicesListbox)
			return;
		
		// Select the slot in the listbox
		m_wSlotChoicesListbox.SetFocus(listboxIndex);
		
		// Trigger the slot selection to show available items
		m_wSlotChoicesListbox.SetFocusedItemSelected();
		
		// Clear hotspots since we're now viewing item choices, not slots
		if (m_wPreviewWidgetComponent)
			m_wPreviewWidgetComponent.ClearSlotHotspots();
		
		PQD_Helpers.PlaySound("click");
	}
}
