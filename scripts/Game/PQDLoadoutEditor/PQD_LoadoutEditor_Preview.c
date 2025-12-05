// PQD Loadout Editor - Preview UI Component
// Author: PQD Team
// Version: 1.0.0
// 100% Original Implementation

//------------------------------------------------------------------------------------------------
//! Slot Hotspot Info - Tracks a clickable hotspot for an attachment slot
class PQD_SlotHotspotInfo
{
	int listboxIndex;           // Index in the slot listbox
	int nodeId;                 // Bone/node ID for this slot
	InventoryStorageSlot slot;  // Reference to the slot
	Widget hotspotWidget;       // The visual clickable widget
	string slotName;            // Name of the slot
}

//------------------------------------------------------------------------------------------------
//! Preview UI Component using native ItemPreviewWidget
class PQD_PreviewUIComponent : ScriptedWidgetComponent
{
	// Widgets
	protected Widget m_wRoot;
	protected ItemPreviewWidget m_wItemPreview;
	protected Widget m_wHotspotContainer; // Container for hotspot buttons
	
	// Preview manager
	protected ItemPreviewManagerEntity m_PreviewManager;
	
	// Entity references
	protected IEntity m_CharacterEntity;
	protected IEntity m_CurrentPreviewEntity;
	protected PQD_SlotInfo m_CurrentSlotInfo;
	
	// Preview attributes for camera control
	protected PreviewRenderAttributes m_PreviewAttributes;
	
	// Mode
	protected PQD_EditorMode m_eCurrentMode;
	
	// Rotation settings
	protected float m_fRotationYaw = 0;
	protected float m_fRotationSpeed = 0.15;
	protected float m_fCurrentZoom = 0;
	
	// Input
	protected InputManager m_pInputManager;
	protected bool m_bIsDragging = false;
	protected bool m_bIsRightDragging = false;
	protected bool m_bPanningEnabled = true;
	protected bool m_bMouseOverPreview = false;
	protected int m_iLastMouseX;
	protected int m_iLastMouseY;
	protected float m_fVerticalOffset = 0;
	
	// Slot Hotspots
	protected ref array<ref PQD_SlotHotspotInfo> m_aSlotHotspots = {};
	protected ref ScriptInvokerInt m_OnSlotHotspotClicked; // Callback when hotspot is clicked
	
	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		Print("[PQD] Preview HandlerAttached", LogLevel.NORMAL);
		m_wRoot = w;
		m_wItemPreview = ItemPreviewWidget.Cast(w.FindAnyWidget("ItemPreview"));
		
		m_pInputManager = GetGame().GetInputManager();
		
		// Get preview manager from world
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world)
			m_PreviewManager = world.GetItemPreviewManager();
	}
	
	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		ClearPreview();
	}
	
	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		Print("[PQD] OnMouseEnter", LogLevel.NORMAL);
		m_bMouseOverPreview = true;
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		m_bMouseOverPreview = false;
		m_bIsDragging = false;
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity characterEntity)
	{
		Print("[PQD] Preview Init", LogLevel.NORMAL);
		m_CharacterEntity = characterEntity;
		m_fRotationYaw = 0;
		m_fCurrentZoom = 0;
		
		// Set initial preview
		if (m_CharacterEntity && m_wItemPreview && m_PreviewManager)
		{
			m_PreviewAttributes = GetPreviewAttributes(m_CharacterEntity);
			if (m_PreviewAttributes)
				m_PreviewAttributes.ResetDeltaRotation();
			
			m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CharacterEntity, m_PreviewAttributes, true);
			m_CurrentPreviewEntity = m_CharacterEntity;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected PreviewRenderAttributes GetPreviewAttributes(IEntity entity)
	{
		if (!entity)
			return null;
		
		InventoryItemComponent inventoryItem = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!inventoryItem)
			return null;
		
		return PreviewRenderAttributes.Cast(inventoryItem.FindAttribute(PreviewRenderAttributes));
	}
	
	//------------------------------------------------------------------------------------------------
	void SetPreviewedEntity(PQD_EditorMode mode, IEntity entity, PQD_SlotInfo slotInfo)
	{
		m_eCurrentMode = mode;
		m_CurrentSlotInfo = slotInfo;
		
		if (!entity || !m_wItemPreview || !m_PreviewManager)
			return;
		
		m_CurrentPreviewEntity = entity;
		
		// Get preview attributes for this entity
		m_PreviewAttributes = GetPreviewAttributes(entity);
		
		// Reset rotation
		m_fRotationYaw = 0;
		
		if (m_PreviewAttributes)
		{
			m_PreviewAttributes.ResetDeltaRotation();
			
			// Zoom in for weapons (smaller items need zoom)
			if (mode == PQD_EditorMode.ENTITY)
			{
				m_fCurrentZoom = 30.0; // Zoom in for weapons
				m_PreviewAttributes.ZoomCamera(m_fCurrentZoom, 15.0, 120.0);
			}
			else
			{
				m_fCurrentZoom = 0;
			}
		}
		
		// Set the preview item
		m_PreviewManager.SetPreviewItem(m_wItemPreview, entity, m_PreviewAttributes, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void Update(float tDelta)
	{
		// Print("[PQD] Preview Update", LogLevel.NORMAL); // Uncomment if needed, but spammy
		if (!m_wRoot || !m_wRoot.IsVisible())
			return;
		
		HandleDragInput(tDelta);
	}
	
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		Print(string.Format("[PQD] OnMouseWheel: %1", wheel), LogLevel.NORMAL);
		
		// Always process zoom when menu is open (don't require mouse over preview)
		// Zoom with mouse wheel
		float zoomSpeed = 3.0;
		m_fCurrentZoom += wheel * zoomSpeed; // + for zoom in with scroll up
		
		// Clamp zoom
		if (m_fCurrentZoom < -30) m_fCurrentZoom = -30; // Zoom in limit
		if (m_fCurrentZoom > 60) m_fCurrentZoom = 60; // Zoom out limit
		
		Print(string.Format("[PQD] Zoom Level: %1", m_fCurrentZoom), LogLevel.NORMAL);
		
		if (m_PreviewAttributes)
			m_PreviewAttributes.ZoomCamera(m_fCurrentZoom, 15.0, 120.0);
			
		// Refresh preview
		if (m_PreviewManager && m_CurrentPreviewEntity && m_wItemPreview)
			m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
			
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleDragInput(float tDelta)
	{
		if (!m_bPanningEnabled || !m_pInputManager)
			return;
		
		// Get current mouse position
		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);
		
		// Check if mouse is over preview widget manually
		if (m_wRoot)
		{
			float posX, posY, sizeX, sizeY;
			m_wRoot.GetScreenPos(posX, posY);
			m_wRoot.GetScreenSize(sizeX, sizeY);
			
			m_bMouseOverPreview = (mouseX >= posX && mouseX <= posX + sizeX && mouseY >= posY && mouseY <= posY + sizeY);
		}
		
		// Get input values
		float zoomInValue = m_pInputManager.GetActionValue("PQD_ZoomIn");
		float zoomOutValue = m_pInputManager.GetActionValue("PQD_ZoomOut");
		float rightDragValue = m_pInputManager.GetActionValue("PQD_RightClick");
		
		// --- ZOOM CONTROL (Mouse Scroll) ---
		// Use the difference between zoomIn and zoomOut as the zoom value
		// Range: diff 36 = zoom in max (-1.0), diff -72 = zoom out max (2.0)
		if (m_bMouseOverPreview && m_CurrentPreviewEntity)
		{
			float zoomDiff = zoomInValue - zoomOutValue;
			
			// Map diff range (36 to -72) to zoom range (-1.0 to 2.0)
			// Formula: zoom = -1.0 + (36 - diff) * (3.0 / 108)
			// Simplified: zoom = (36 - diff) / 36 - 1 = 1 - diff/36 - 1 = -diff/36
			// But we need: diff=36 -> -1, diff=-72 -> 2
			// Linear mapping: zoom = -1 + (36 - diff) * 3 / 108
			float newZoom = -1.0 + (36.0 - zoomDiff) * 3.0 / 108.0;
			
			// Only update if there's actual scroll activity
			static float lastZoomDiff = 0;
			if (zoomDiff != lastZoomDiff)
			{
				m_fCurrentZoom = newZoom;
				
				// Clamp zoom
				if (m_fCurrentZoom < -1.0) m_fCurrentZoom = -1.0;
				if (m_fCurrentZoom > 2.0) m_fCurrentZoom = 2.0;
				
				Print(string.Format("[PQD] Zoom: diff=%1, zoom=%2", zoomDiff, m_fCurrentZoom), LogLevel.NORMAL);
				ApplyZoom();
				
				lastZoomDiff = zoomDiff;
			}
		}
			
		// --- ZOOM CONTROL (Gamepad) ---
		float zoomInput = m_pInputManager.GetActionValue("PQD_Zoom");
		if (zoomInput != 0 && m_CurrentPreviewEntity)
		{
			Print(string.Format("[PQD] Gamepad Zoom Input: %1", zoomInput), LogLevel.NORMAL);
			float zoomSpeed = 1.0 * tDelta; // Zoom speed for gamepad
			m_fCurrentZoom -= zoomInput * zoomSpeed;
			
			// Clamp zoom
			if (m_fCurrentZoom < -1.0) m_fCurrentZoom = -1.0;
			if (m_fCurrentZoom > 2.0) m_fCurrentZoom = 2.0;
			
			ApplyZoom();
		}
		
		// --- ROTATION CONTROL (Gamepad) ---
		float rotateYaw = m_pInputManager.GetActionValue("PQD_RotateYaw");
		float rotatePitch = m_pInputManager.GetActionValue("PQD_RotatePitch");
		
		if (rotateYaw != 0 || rotatePitch != 0)
		{
			Print(string.Format("[PQD] Rotate Input: Yaw=%1, Pitch=%2", rotateYaw, rotatePitch), LogLevel.NORMAL);
			if (m_PreviewAttributes)
			{
				float gamepadRotSpeed = 50.0 * tDelta; // Reduced speed
				vector deltaRotation = Vector(-rotatePitch * gamepadRotSpeed, rotateYaw * gamepadRotSpeed, 0); // Inverted pitch
				vector limitMin = Vector(-90, -360, 0);  // Pitch limit 90°, Yaw 360°
				vector limitMax = Vector(90, 360, 0);
				
				m_PreviewAttributes.RotateItemCamera(deltaRotation, limitMin, limitMax);
				
				// Refresh preview
				if (m_PreviewManager && m_CurrentPreviewEntity && m_wItemPreview)
					m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
			}
		}
		
		// --- VERTICAL PAN CONTROL (Right Mouse Button Drag) ---
		// rightDragValue already declared above in debug section
		
		if (rightDragValue > 0.1 && m_bMouseOverPreview && m_CurrentPreviewEntity)
		{
			if (m_bIsRightDragging)
			{
				// Calculate delta movement
				int deltaY = mouseY - m_iLastMouseY;
				
				if (deltaY != 0)
				{
					Print(string.Format("[PQD] Vertical Pan: deltaY=%1", deltaY), LogLevel.NORMAL);
					ApplyVerticalPan(deltaY);
				}
			}
			
			m_bIsRightDragging = true;
			m_iLastMouseX = mouseX;
			m_iLastMouseY = mouseY;
		}
		else
		{
			m_bIsRightDragging = false;
		}
		
		// --- ROTATION CONTROL (Mouse Left Button Drag) ---
		// Check for left mouse button drag
		float dragValue = m_pInputManager.GetActionValue("Inventory_Drag");
		
		if (dragValue > 0.1 && m_bMouseOverPreview && rightDragValue <= 0.1)
		{
			if (m_bIsDragging)
			{
				// Calculate delta movement
				int deltaX = mouseX - m_iLastMouseX;
				int deltaY = mouseY - m_iLastMouseY;
				
				if (m_PreviewAttributes && (deltaX != 0 || deltaY != 0))
				{
					// Apply rotation to camera (deltaX = yaw, -deltaY = pitch inverted)
					vector deltaRotation = Vector(-deltaY * m_fRotationSpeed, deltaX * m_fRotationSpeed, 0);
					vector limitMin = Vector(-90, -360, 0);  // Pitch limit 90°, Yaw 360°
					vector limitMax = Vector(90, 360, 0);
					
					m_PreviewAttributes.RotateItemCamera(deltaRotation, limitMin, limitMax);
					
					// Refresh preview
					if (m_PreviewManager && m_CurrentPreviewEntity && m_wItemPreview)
						m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
					
					// Update slot hotspot positions to follow the 3D model
					UpdateSlotHotspotPositions();
				}
			}
			
			m_bIsDragging = true;
			m_iLastMouseX = mouseX;
			m_iLastMouseY = mouseY;
		}
		else
		{
			m_bIsDragging = false;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyZoom()
	{
		if (!m_CurrentPreviewEntity || !m_PreviewAttributes)
			return;
		
		// Use ZoomCamera for zoom effect (this method is confirmed to exist)
		// zoom value: negative = zoom in, positive = zoom out
		float zoomValue = m_fCurrentZoom * 30.0; // Scale to camera FOV range
		m_PreviewAttributes.ZoomCamera(zoomValue, 15.0, 120.0);
		
		// Refresh preview to apply changes
		if (m_PreviewManager && m_wItemPreview)
			m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyVerticalPan(int deltaY)
	{
		if (!m_CurrentPreviewEntity || !m_PreviewAttributes)
			return;
		
		// Apply vertical rotation (pitch) based on deltaY - NOT accumulated, just the delta
		float pitchSpeed = 0.1;
		float pitchDelta = -deltaY * pitchSpeed;
		
		// Use RotateItemCamera with only pitch delta
		vector deltaRotation = Vector(pitchDelta, 0, 0);
		vector limitMin = Vector(-45, -360, 0);  // Limit pitch to 45 degrees
		vector limitMax = Vector(45, 360, 0);
		m_PreviewAttributes.RotateItemCamera(deltaRotation, limitMin, limitMax);
		
		// Refresh preview to apply changes
		if (m_PreviewManager && m_wItemPreview)
			m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetPanningEnabled(bool enabled)
	{
		m_bPanningEnabled = enabled;
		if (!enabled)
			m_bIsDragging = false;
	}
	
	//------------------------------------------------------------------------------------------------
	void ClearPreview()
	{
		if (m_wItemPreview && m_PreviewManager)
			m_PreviewManager.SetPreviewItem(m_wItemPreview, null, null, false);
		
		m_CurrentPreviewEntity = null;
		m_PreviewAttributes = null;
	}
	
	//------------------------------------------------------------------------------------------------
	IEntity GetPreviewEntity()
	{
		return m_CurrentPreviewEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	void ResetRotation()
	{
		m_fRotationYaw = 0;
		if (m_PreviewAttributes)
		{
			m_PreviewAttributes.ResetDeltaRotation();
			
			// Refresh preview
			if (m_PreviewManager && m_CurrentPreviewEntity && m_wItemPreview)
				m_PreviewManager.SetPreviewItem(m_wItemPreview, m_CurrentPreviewEntity, m_PreviewAttributes, true);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// SLOT HOTSPOT SYSTEM
	//------------------------------------------------------------------------------------------------
	
	//------------------------------------------------------------------------------------------------
	//! Get the slot hotspot clicked invoker
	ScriptInvokerInt GetOnSlotHotspotClicked()
	{
		if (!m_OnSlotHotspotClicked)
			m_OnSlotHotspotClicked = new ScriptInvokerInt();
		return m_OnSlotHotspotClicked;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create clickable hotspots for each slot in the current entity
	void CreateSlotHotspots(array<ref PQD_SlotInfo> slotInfos)
	{
		// Clear existing hotspots first
		ClearSlotHotspots();
		
		Print(string.Format("[PQD] CreateSlotHotspots: slotInfos count=%1", slotInfos.Count()), LogLevel.NORMAL);
		
		if (!m_CurrentPreviewEntity || !m_wItemPreview)
		{
			Print("[PQD] CreateSlotHotspots: No preview entity or widget", LogLevel.WARNING);
			return;
		}
		
		// Get hotspot container
		if (!m_wHotspotContainer)
		{
			m_wHotspotContainer = m_wRoot.FindAnyWidget("HotspotContainer");
			if (!m_wHotspotContainer)
			{
				Print("[PQD] HotspotContainer widget not found in preview layout", LogLevel.WARNING);
				return;
			}
		}
		
		// Create hotspot for each slot
		foreach (int i, PQD_SlotInfo slotInfo : slotInfos)
		{
			if (!slotInfo)
			{
				Print(string.Format("[PQD] SlotInfo %1 is null", i), LogLevel.WARNING);
				continue;
			}
			
			// Get the node ID for this slot (may be -1 if not set)
			int nodeId = -1;
			if (slotInfo.slot)
				nodeId = slotInfo.slot.GetNodeId();
			
			Print(string.Format("[PQD] Slot %1: name=%2, nodeId=%3", i, slotInfo.slotName, nodeId), LogLevel.NORMAL);
			
			// Create hotspot info
			PQD_SlotHotspotInfo hotspotInfo = new PQD_SlotHotspotInfo();
			hotspotInfo.listboxIndex = i;
			hotspotInfo.nodeId = nodeId;
			hotspotInfo.slot = slotInfo.slot;
			hotspotInfo.slotName = slotInfo.slotName;
			
			// Create a button widget for the hotspot
			Widget newHotspot = GetGame().GetWorkspace().CreateWidgets("{36A427D18C762655}UI/PQDLoadoutEditor/PQD_SlotHotspot.layout", m_wHotspotContainer);
			if (newHotspot)
			{
				hotspotInfo.hotspotWidget = newHotspot;
				
				// Store index for click handling - will be checked in OnClick
				newHotspot.SetZOrder(100 + i); // Use 100+ to distinguish from other widgets
				
				// Initially hide - will be positioned and shown in UpdateSlotHotspotPositions
				newHotspot.SetVisible(false);
				
				m_aSlotHotspots.Insert(hotspotInfo);
			}
			else
			{
				Print("[PQD] Failed to create hotspot widget", LogLevel.ERROR);
			}
		}
		
		Print(string.Format("[PQD] Created %1 slot hotspots", m_aSlotHotspots.Count()), LogLevel.NORMAL);
	}

	
	//------------------------------------------------------------------------------------------------
	//! Update hotspot positions based on 3D model bone positions
	void UpdateSlotHotspotPositions()
	{
		if (!m_wItemPreview || !m_CurrentPreviewEntity || m_aSlotHotspots.Count() == 0)
			return;
		
		vector offset[4];
		Math3D.MatrixIdentity4(offset);
		
		foreach (PQD_SlotHotspotInfo hotspotInfo : m_aSlotHotspots)
		{
			if (!hotspotInfo || !hotspotInfo.hotspotWidget)
				continue;
			
			vector posInWidget;
			
			// Try to get the position of this node in widget space
			if (m_wItemPreview.TryGetItemNodePositionInWidgetSpace(hotspotInfo.nodeId, offset, posInWidget))
			{
				// Position the hotspot widget
				float x = posInWidget[0];
				float y = posInWidget[1];
				
				// Center the hotspot on the position
				float hotspotSize = 24; // Size of hotspot widget
				FrameSlot.SetPos(hotspotInfo.hotspotWidget, x - hotspotSize * 0.5, y - hotspotSize * 0.5);
				
				hotspotInfo.hotspotWidget.SetVisible(true);
			}
			else
			{
				// Node not visible, hide the hotspot
				hotspotInfo.hotspotWidget.SetVisible(false);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clear all slot hotspots
	void ClearSlotHotspots()
	{
		foreach (PQD_SlotHotspotInfo hotspotInfo : m_aSlotHotspots)
		{
			if (hotspotInfo && hotspotInfo.hotspotWidget)
				hotspotInfo.hotspotWidget.RemoveFromHierarchy();
		}
		m_aSlotHotspots.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle hotspot button click
	void OnHotspotClicked(int listboxIndex)
	{
		Print(string.Format("[PQD] Hotspot clicked: index=%1", listboxIndex), LogLevel.NORMAL);
		
		if (m_OnSlotHotspotClicked)
			m_OnSlotHotspotClicked.Invoke(listboxIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override OnClick to handle hotspot button clicks
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;
		
		// Check if this is one of our hotspot buttons
		foreach (PQD_SlotHotspotInfo hotspotInfo : m_aSlotHotspots)
		{
			if (hotspotInfo && hotspotInfo.hotspotWidget == w)
			{
				OnHotspotClicked(hotspotInfo.listboxIndex);
				return true;
			}
		}
		
		return false;
	}
}
