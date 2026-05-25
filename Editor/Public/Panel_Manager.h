#pragma once

#include "Editor_Defines.h"
#include "Base.h"

#include "Panel.h"
#include "Panel_ContentBrowser.h"
#include "Panel_Hierarchy.h"
#include "Panel_Inspector.h"
#include "Panel_Log.h"
#include "Panel_Viewport.h"
#include "Panel_Shortcuts.h"
#include "Panel_NavMeshEditor.h"
#include "Panel_2DCanvas.h"

NS_BEGIN(Engine)
class CGameObject;
NS_END

NS_BEGIN(Editor)

class CPanel;

class CPanel_Manager final : public CBase
{
	DECLARE_SINGLETON(CPanel_Manager)
private:
	CPanel_Manager();
	virtual ~CPanel_Manager() = default;

public:
	CGameObject*			Get_SelectedObject() const 
	{ 
		return m_pSelectedObject; 
	}

public:
	EDITOR_TOOL_MODE		Get_Tool_Mode() const { return m_eToolMode; }
	_bool					Is_NavMeshEditMode() const { return EDITOR_TOOL_MODE::NAVMESH == m_eToolMode; }
	_bool					Is_UICanvasMode() const { return EDITOR_TOOL_MODE::UI_CANVAS == m_eToolMode; }
	_bool					Is_LightEditMode() const { return EDITOR_TOOL_MODE::LIGHT == m_eToolMode; }
	_bool					Is_CamColliderMode() const { return EDITOR_TOOL_MODE::CAM_COLLIDER == m_eToolMode; }

	void					Set_ToolMode(EDITOR_TOOL_MODE eMode);

public:
	HRESULT					Add_Panel(const _wstring& strPanelTag, CPanel* pPanel);
	CPanel*					Get_Panel(const _wstring& strPanelTag);
	void					Update_Panels(_float fTimeDelta);
	void					Render_Panels();

	void					Clear_Selection();
	void					Set_SelectedObject(CGameObject* pObject);
	void					Release_Panels();

private:
	map<_wstring, CPanel*>	m_Panels;
	CGameObject*			m_pSelectedObject = { nullptr };

	EDITOR_TOOL_MODE		m_eToolMode = { EDITOR_TOOL_MODE::OBJECT };

public:
	virtual void Free() override;
};

NS_END
