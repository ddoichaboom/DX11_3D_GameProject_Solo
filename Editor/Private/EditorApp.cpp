#include "EditorApp.h"
#include "GameInstance.h"
#include "Panel_Manager.h"

#include "AnimController.h"
#include "Level_Loading.h"
#include "Camera_Free.h"
#include "SpringArm.h"
#include "Panel_2DCanvas.h"
#include "UICanvasTool.h"
#include "Layer.h"
#include "Camera.h"


CEditorApp::CEditorApp()
	: m_pGameInstance { CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}
#pragma region EDITOR

HRESULT	CEditorApp::Initialize(HWND hWnd, HINSTANCE hInstance, _uint iWinSizeX, _uint iWinSizeY)
{
	// 엔진 초기화
	ENGINE_DESC		EngineDesc{};
	EngineDesc.hWnd				= hWnd;
	EngineDesc.hInstance		= hInstance;
	EngineDesc.eWinMode			= WINMODE::WIN;
	EngineDesc.iViewportWidth	= iWinSizeX;
	EngineDesc.iViewportHeight	= iWinSizeY;
	EngineDesc.iNumLevels		= 5; 

	if (FAILED(m_pGameInstance->Initialize_Engine(EngineDesc, &m_pDevice, &m_pContext)))
	{
		MSG_BOX("Failed to Initialize : Engine");
		return E_FAIL;
	}

	// ImGui 초기화 (엔진 초기화 이후)
	if (FAILED(Ready_ImGui(hWnd)))
	{
		MSG_BOX("Failed to Initialize : ImGui");
		return E_FAIL;
	}

	if (FAILED(Ready_Panels()))
	{
		MSG_BOX("Failed to Initialize : Panels");
		return E_FAIL;
	}

	if (FAILED(Ready_BootScene()))
	{
		MSG_BOX("Failed to Initialize : TestScene");
		return E_FAIL;
	}

	return S_OK;
}

void	CEditorApp::Update(_float fTimeDelta)
{
	m_pGameInstance->Update_Engine(fTimeDelta);

	// GAMEPLAY 도달 직후 한 번 - Camera_Free 인스턴스 + 초기 모드 적용
	if (false == m_bSceneBootstrapped &&
		ETOUI(LEVEL::GAMEPLAY) == m_pGameInstance->Get_CurrentLevelIndex())
	{
		Bootstrap_EditScene();
		m_bSceneBootstrapped = true;
	}

	// F5 토글 
	if (m_bSceneBootstrapped && m_pGameInstance->Get_KeyDown(VK_F5))
		Toggle_Mode();

	m_pPanel_Manager->Update_Panels(fTimeDelta);
}

HRESULT	CEditorApp::Render()
{
	// (1) 별도 RT에 3D Scene 렌더
	if (FAILED(Render_Scene()))
		return E_FAIL;

	// (2) BackBuffer Clear -> ImGui 전용
	if (FAILED(m_pGameInstance->Begin_Draw()))
		return E_FAIL;

	// (3) ImGui 프레임 시작
	Begin_ImGuiFrame();

	// (4) DockSpace + MenuBar
	Render_DockSpace();

	// (5) 패널 UI 렌더 (Viewport 패널에서 ImGui::Image(SRV)로 3D 표시
	m_pPanel_Manager->Render_Panels();

	// (6) ImGui Draw
	Render_ImGui();

	// (7) Present
	if (FAILED(m_pGameInstance->End_Draw()))
		return E_FAIL;

	return S_OK;
}

#pragma endregion

#pragma region IMGUI

HRESULT CEditorApp::Ready_ImGui(HWND hWnd)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();


	// ImGuizmo가 사용할 ImGui 컨텍스트 지정 
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // 멀티 뷰포트 (필요 시)

	ImGui::StyleColorsDark();

	io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/malgun.ttf",
		16.f,
		nullptr,
		io.Fonts->GetGlyphRangesKorean());

	if (!ImGui_ImplWin32_Init(hWnd))
		return E_FAIL;

	if (!ImGui_ImplDX11_Init(m_pDevice, m_pContext))
		return E_FAIL;

	return S_OK;
}

void CEditorApp::ShutDown_ImGui()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void CEditorApp::Begin_ImGuiFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// ImGuizmo 프레임 시작 - 매 프레임 NewFrame 직후 반드시 호출
	ImGuizmo::BeginFrame();
}

void CEditorApp::Render_DockSpace()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

	// 최초 1회만 레이아웃 설정 (imgui.ini가 없을 때)
	if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
	{
		ImGui::DockBuilderRemoveNode(dockspace_id);
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

		ImGuiID left, center, right, bottom;
		ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, &left, &center);
		ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.2f, &right, &center);
		ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, &bottom, &center);

		ImGui::DockBuilderDockWindow("Hierarchy", left);
		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("Content Browser", bottom);
		ImGui::DockBuilderDockWindow("Log", bottom);  
		ImGui::DockBuilderDockWindow("Shortcuts", bottom);  
		ImGui::DockBuilderDockWindow("NavMesh Editor", right);  

		ImGui::DockBuilderFinish(dockspace_id);
	}

	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// Menu Bar (2-4)
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Window"))
		{
			ToggleMenuItem(TEXT("Panel_Viewport"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_Hierarchy"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_Inspector"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_ContentBrowser"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_Log"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_Shortcuts"), MENUTYPE::PANEL);
			ToggleMenuItem(TEXT("Panel_NavMeshEditor"), MENUTYPE::PANEL);

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
}

void CEditorApp::Render_ImGui()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CEditorApp::ToggleMenuItem(const _wstring& strMenuTag, MENUTYPE eType)
{
	
	if (MENUTYPE::PANEL == eType)
	{
		CPanel* pPanel = m_pPanel_Manager->Get_Panel(strMenuTag);
		if (ImGui::MenuItem(pPanel->Get_Name(), nullptr, pPanel->Is_Open()))
			pPanel->Set_Open(!pPanel->Is_Open());
	}
	//else if (MENUTYPE::TOOL == eType)
	//{
	//	// 
	//}
		
}
#pragma endregion

#pragma region PANEL_MANAGER

HRESULT CEditorApp::Ready_Panels()
{
	m_pPanel_Manager = CPanel_Manager::GetInstance();
	Safe_AddRef(m_pPanel_Manager);

	m_pViewport = CPanel_Viewport::Create(m_pDevice, m_pContext);

	// Panel_Viewport
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_Viewport"), m_pViewport)))
		return E_FAIL;

	Safe_AddRef(m_pViewport);


	// Panel_NavMeshEditor
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_NavMeshEditor"), CPanel_NavMeshEditor::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_2DCanvas
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_2DCanvas"), CPanel_2DCanvas::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_Hierarchy
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_Hierarchy"), CPanel_Hierarchy::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_Inspector
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_Inspector"), CPanel_Inspector::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_ContentBrowser
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_ContentBrowser"), CPanel_ContentBrowser::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_Log
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_Log"), CPanel_Log::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Panel_Shortcuts
	if (FAILED(m_pPanel_Manager->Add_Panel(TEXT("Panel_Shortcuts"), CPanel_Shortcuts::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	return S_OK;
}

#pragma endregion

#pragma region SCENE_RENDER

HRESULT CEditorApp::Render_Scene()
{
	if (FAILED(m_pViewport->Begin_RT()))
		return S_OK;

	if (FAILED(m_pGameInstance->Draw()))
	{
#ifdef _DEBUG
		OutputDebugString(TEXT("[EditorApp] GameInstance::Draw failed in Render_Scene.\n"));
#endif
		m_pViewport->End_RT();
		return E_FAIL;
	}

	if (m_pPanel_Manager->Is_UICanvasMode())
	{
		CPanel* pPanel = m_pPanel_Manager->Get_Panel(TEXT("Panel_2DCanvas"));
		CPanel_2DCanvas* pCanvas = dynamic_cast<CPanel_2DCanvas*>(pPanel);
		if (nullptr != pCanvas && nullptr != pCanvas->Get_Tool())
		{
			pCanvas->Get_Tool()->Render_TextPreview_ToRT(
				m_pViewport->Get_RTWidth(),
				m_pViewport->Get_RTHeight());
		}
	}

	if (FAILED(m_pViewport->End_RT()))
		return E_FAIL;

	return S_OK;
}

HRESULT CEditorApp::Ready_BootScene()
{
#pragma region ADD_FONT
	// 영문/숫자 강조 - HUD 수치, 데미지, 카운터
	if (FAILED(m_pGameInstance->Add_Font(TEXT("Font_NumericEN"),
		TEXT("../../Resources/Fonts/agencyb_48.spritefont"))))
		return E_FAIL;

	// 굵은 한글 제목/버튼/메뉴 헤더
	if (FAILED(m_pGameInstance->Add_Font(TEXT("Font_HeaderKR"),
		TEXT("../../Resources/Fonts/nanumsquareb_48.spritefont"))))
		return E_FAIL;

	// 굵은 한글 일반 UI - 리스트, 라벨
	if (FAILED(m_pGameInstance->Add_Font(TEXT("Font_LabelKR"),
		TEXT("../../Resources/Fonts/nanumgothic_32.spritefont"))))
		return E_FAIL;

	// 기본 한글 본문 - 시스템 메시지, 범용 UI (Default)
	if (FAILED(m_pGameInstance->Add_Font(TEXT("Font_Default"),
		TEXT("../../Resources/Fonts/notosanskr_32.spritefont"))))
		return E_FAIL;

	// 한글 강조/ 포인트 텍스트 (둥근 느낌)
	if (FAILED(m_pGameInstance->Add_Font(TEXT("Font_AccentKR"),
		TEXT("../../Resources/Fonts/binggrae2_32.spritefont"))))
		return E_FAIL;
#pragma endregion

	if (FAILED(m_pGameInstance->Add_Prototype(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_VIBuffer_Rect"),
		CVIBuffer_Rect::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_Shader_VtxTex"),
		CShader::Create(m_pDevice, m_pContext,
			TEXT("../../Resources/ShaderFiles/Shader_VtxTex.hlsl"),
			VTXTEX::Elements, VTXTEX::iNumElements))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(
		ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_AnimController"),
		CAnimController::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_Prototype(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_SpringArm"),
		CSpringArm::Create(m_pDevice, m_pContext))))
		return E_FAIL;

	// Client Loader 가 GAMEPLAY 리소스를 로드하고, Level_Loading이 완료 시 Level_GamePlay로 자동 전이
	if (FAILED(m_pGameInstance->Change_Level(
		ETOUI(LEVEL::LOADING),
		CLevel_Loading::Create(m_pDevice, m_pContext, LEVEL::GAMEPLAY))))
		return E_FAIL;

	return S_OK;
}

#pragma endregion

#pragma region CAMERA

void CEditorApp::Set_CameraActive(const _wstring& strCameraTag, _bool bActive)
{
	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return;

	auto iterLayer = pLayers->find(TEXT("Layer_Camera"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return;

	const list<CGameObject*>& Cameras = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : Cameras)
	{
		if (nullptr == pObject)
			continue;

		if (pObject->Get_Tag() != strCameraTag)
			continue;

		if (CCamera* pCamera = dynamic_cast<CCamera*>(pObject))
			pCamera->Set_ActiveCamera(bActive);
	}
}

#pragma endregion

void CEditorApp::Bootstrap_EditScene()
{
	// Camera_Free 인스턴스 
	CCamera_Free::CAMERA_FREE_DESC Desc{};
	Desc.vEye				= _float3(0.f, 10.f, -7.f);
	Desc.vAt				= _float3(0.f, 0.f, 0.f);
	Desc.fFovy				= XMConvertToRadians(60.f);
	Desc.fNear				= 0.1f;
	Desc.fFar				= 500.f;
	Desc.fSpeedPerSec		= 10.f;
	Desc.fRotationPerSec	= XMConvertToRadians(180.f);
	Desc.fMouseSensor		= 0.05f;

	m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_Camera_Free"),
		ETOUI(LEVEL::GAMEPLAY), TEXT("Layer_Camera"), &Desc);

	Apply_Mode();
}

void CEditorApp::Toggle_Mode()
{
	m_bEditMode = !m_bEditMode;
	Apply_Mode();
}

void CEditorApp::Apply_Mode()
{
	m_pGameInstance->Set_GameLogic_Frozen(m_bEditMode);
	m_pGameInstance->Set_CursorLocked(!m_bEditMode);

	Set_CameraActive(TEXT("Follow"), false == m_bEditMode);
	Set_CameraActive(TEXT("Free"), true == m_bEditMode);

	if (nullptr != m_pViewport)
		m_pViewport->Resize_ActiveCameraProjection();
}

CEditorApp* CEditorApp::Create(HWND hWnd, HINSTANCE hInstance, _uint iWinSizeX, _uint iWinSizeY)
{
	CEditorApp* pInstance = new CEditorApp();

	if (FAILED(pInstance->Initialize(hWnd, hInstance, iWinSizeX, iWinSizeY)))
	{
		MSG_BOX("Failed to Created : CEditorApp");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CEditorApp::Free() 
{
	__super::Free();

	ShutDown_ImGui();

	Safe_Release(m_pViewport);

	m_pPanel_Manager->Release_Panels();
	Safe_Release(m_pPanel_Manager);

	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);

	m_pGameInstance->Release_Engine();
	Safe_Release(m_pGameInstance);
}
