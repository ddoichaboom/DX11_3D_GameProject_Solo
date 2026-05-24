#pragma once

#include "Editor_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)
class CGameInstance;
NS_END

NS_BEGIN(Editor)

class CPanel_Manager;
class CPanel_Viewport;

class CEditorApp final : public CBase
{
private:
	CEditorApp();
	virtual ~CEditorApp() = default;

#pragma region EDITOR
public:
	HRESULT					Initialize(HWND hWnd, HINSTANCE hInstance, _uint iWinSizeX, _uint iWinSizeY);
	void					Update(_float fTimeDelta);
	HRESULT					Render();
#pragma endregion 

#pragma region IMGUI
private:
	HRESULT					Ready_ImGui(HWND hWnd);
	void					ShutDown_ImGui();

	// ImGui Render ÇÔ¼öµé 
	void					Begin_ImGuiFrame();
	void					Render_DockSpace();
	void					Render_ImGui();
	void					ToggleMenuItem(const _wstring& strMenuTag, MENUTYPE eType);
#pragma endregion

#pragma region PANEL_MANAGER
private:
	HRESULT					Ready_Panels();
#pragma endregion

#pragma region SCENE_RENDER

private:
	HRESULT					Render_Scene(); 
	HRESULT					Ready_BootScene();

#pragma endregion 

#pragma region CAMERA
private:
	void					Set_CameraActive(const _wstring& strCameraTag, _bool bActive);
#pragma endregion 
private:
	ID3D11Device*			m_pDevice				= { nullptr };
	ID3D11DeviceContext*	m_pContext				= { nullptr };
	CGameInstance*			m_pGameInstance			= { nullptr };

	CPanel_Manager*			m_pPanel_Manager		= { nullptr };
	CPanel_Viewport*		m_pViewport				= { nullptr };

private:
	_bool					m_bEditMode				= { true };
	_bool					m_bSceneBootstrapped	= { false };

private:
	void					Bootstrap_EditScene();
	void					Toggle_Mode();
	void					Apply_Mode();
	

public:
	static CEditorApp*		Create(HWND hWnd, HINSTANCE hInstance, _uint iWinSizeX, _uint iWinSizeY);
	virtual void			Free() override;
};

NS_END