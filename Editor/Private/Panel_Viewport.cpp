#include "Panel_Viewport.h"
#include "GameInstance.h"
#include "Layer.h"
#include "GameObject.h"
#include "Panel_Manager.h"
#include "Model.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "VIBuffer.h"
#include "Panel_NavMeshEditor.h"
#include "NavMeshEditorTool.h"
#include "Panel_2DCanvas.h"
#include "UICanvasTool.h"
#include "Camera.h"

namespace
{
	CNavMeshEditorTool* Find_NavMeshEditorTool(CPanel_Manager* pPanelManager)
	{
		if (nullptr == pPanelManager)
			return nullptr;

		CPanel* pPanel = pPanelManager->Get_Panel(TEXT("Panel_NavMeshEditor"));
		CPanel_NavMeshEditor* pNavMeshEditor = dynamic_cast<CPanel_NavMeshEditor*>(pPanel);
		if (nullptr == pNavMeshEditor)
			return nullptr;

		return pNavMeshEditor->Get_Tool();
	}
}
CPanel_Viewport::CPanel_Viewport(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPanel{ pDevice, pContext }
{
}

HRESULT CPanel_Viewport::Initialize()
{
	strcpy_s(m_szName, "Viewport");

	m_iRTWidth = 1280;
	m_iRTHeight = 720;

	return S_OK;
}

void CPanel_Viewport::Update(_float fTimeDelta)
{
}

void CPanel_Viewport::Render()
{
	ImGui::Begin(m_szName, &m_bOpen);

	//  패널 내부 가용 크기 획득
	ImVec2 vAvail = ImGui::GetContentRegionAvail();

	// 리사이즈 감지
	if (vAvail.x > 0.f && vAvail.y > 0.f)
	{
		_uint iNewWidth = static_cast<_uint>(vAvail.x);
		_uint iNewHeight = static_cast<_uint>(vAvail.y);

		if (iNewWidth != m_iRTWidth || iNewHeight != m_iRTHeight)
		{
			m_iRTWidth = iNewWidth;
			m_iRTHeight = iNewHeight;

			m_pGameInstance->Resize_RenderTargets(iNewWidth, iNewHeight);
			Resize_ActiveCameraProjection();
		}
	}

	ID3D11ShaderResourceView* pViewportSRV = Get_SRV();

	if (nullptr != pViewportSRV)
	{
		ImVec2 vImagePos = ImGui::GetCursorScreenPos();		// Image 좌상단 좌표

		ImGui::Image(
			reinterpret_cast<ImTextureID>(pViewportSRV),
			ImVec2(static_cast<_float>(m_iRTWidth), static_cast<_float>(m_iRTHeight)));

		const _bool bViewportImageHovered = ImGui::IsItemHovered();

		const _bool bWindowFocused =
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		const _bool bNavMeshToolbarHovered = false;
		CNavMeshEditorTool* pNavMeshEditorTool = Find_NavMeshEditorTool(m_pPanel_Manager);
		const _bool bNavMeshEditMode = m_pPanel_Manager->Is_NavMeshEditMode();
		const _bool bLightEditMode = m_pPanel_Manager->Is_LightEditMode();
		CUICanvasTool* pUICanvasTool = Find_UICanvasTool();
		const _bool bUICanvasMode = m_pPanel_Manager->Is_UICanvasMode();

		// ImGuizmo 오버레이 세팅
		// (1) 기즈모 드로잉을 현재 Viewport 윈도우 drawlist에 연결
		ImGuizmo::SetDrawlist();

		// (2) 기즈모 수학이 사용할 스크린 공간 영역 = Image 위젯 영역과 일치
		ImGuizmo::SetRect(
			vImagePos.x, vImagePos.y,
			static_cast<_float>(m_iRTWidth),
			static_cast<_float>(m_iRTHeight));

		if (bWindowFocused &&
			bNavMeshEditMode &&
			false == ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
			false == ImGui::IsAnyItemActive() &&
			false == ImGui::IsKeyDown(ImGuiMod_Ctrl) &&
			false == ImGui::IsKeyDown(ImGuiMod_Alt) &&
			false == ImGui::IsKeyDown(ImGuiMod_Shift) &&
			ImGui::IsKeyPressed(ImGuiKey_C))
		{
			if (nullptr != pNavMeshEditorTool)
				pNavMeshEditorTool->Create_NavMeshCell();
		}

		// 기즈모 단축키 처리
		// Viewport 포커스 상태 + RMB(카메라 모드) 비활성 시에만 반응
		if (bWindowFocused && !bNavMeshEditMode && !bLightEditMode && !bUICanvasMode  && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W))
				m_eGizmoOperation = ImGuizmo::TRANSLATE;
			else if (ImGui::IsKeyPressed(ImGuiKey_E))
				m_eGizmoOperation = ImGuizmo::ROTATE;
			else if (ImGui::IsKeyPressed(ImGuiKey_R))
				m_eGizmoOperation = ImGuizmo::SCALE;

			if (ImGui::IsKeyPressed(ImGuiKey_X))
			{
				m_eGizmoMode = (m_eGizmoMode == ImGuizmo::LOCAL)
					? ImGuizmo::WORLD
					: ImGuizmo::LOCAL;
			}
		}

		_bool bGizmoBlocking = { false };

		if (false == bNavMeshEditMode && false == bUICanvasMode)
		{
			// 선택 오브젝트에 대한 기즈모 조작
			CGameObject* pSelected = m_pPanel_Manager->Get_SelectedObject();
			if (nullptr != pSelected)
			{
				CTransform* pTransform = pSelected->Get_Transform();
				if (nullptr != pTransform)
				{
					// View/Proj 행렬
					const _float4x4* pViewMatrix = m_pGameInstance->Get_Transform(D3DTS::VIEW);
					const _float4x4* pProjMatrix = m_pGameInstance->Get_Transform(D3DTS::PROJ);

					// 대상 World 행렬을 스택 로컬로 복사
					_float4x4 worldMatrix = *pTransform->Get_WorldMatrixPtr();

					const _float* pSnap = { nullptr };
					_float3 vSnap = {};

					if (ImGui::IsKeyDown(ImGuiMod_Ctrl))
					{
						switch (m_eGizmoOperation)
						{
						case ImGuizmo::TRANSLATE:
							vSnap = _float3(m_fSnapTranslate, m_fSnapTranslate, m_fSnapTranslate);
							break;
						case ImGuizmo::ROTATE:
							vSnap = _float3(m_fSnapRotate, m_fSnapRotate, m_fSnapRotate);
							break;
						case ImGuizmo::SCALE:
							vSnap = _float3(m_fSnapScale, m_fSnapScale, m_fSnapScale);
							break;
						}
						pSnap = reinterpret_cast<const _float*>(&vSnap);
					}

					// 기즈모 조작
					ImGuizmo::Manipulate(
						reinterpret_cast<const _float*>(pViewMatrix),
						reinterpret_cast<const _float*>(pProjMatrix),
						m_eGizmoOperation,
						m_eGizmoMode,
						reinterpret_cast<_float*>(&worldMatrix),
						nullptr,
						pSnap);

					// 조작이 발생했을 때만 Transform에 반영
					if (ImGuizmo::IsUsing())
						pTransform->Set_WorldMatrix(worldMatrix);

					// 이 프레임에 Manipulate를 호출했을 때만 IsOver/IsUsing 상태가 유효
					bGizmoBlocking = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
				}
			}
		}

		if (bViewportImageHovered &&
			false == bNavMeshToolbarHovered &&
			false == bUICanvasMode &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!bGizmoBlocking)
		{
			ImVec2 vMousePos = ImGui::GetMousePos();
			m_fPickX = vMousePos.x - vImagePos.x;
			m_fPickY = vMousePos.y - vImagePos.y;

			if (bNavMeshEditMode)
			{
				if (nullptr != pNavMeshEditorTool)
					pNavMeshEditorTool->Handle_ViewportClick(m_fPickX, m_fPickY, m_iRTWidth, m_iRTHeight);
			}
			else if (bLightEditMode)
			{
				if (nullptr != pNavMeshEditorTool)
					pNavMeshEditorTool->Handle_LightViewportClick(m_fPickX, m_fPickY, vImagePos, m_iRTWidth, m_iRTHeight);
			}
			else
			{
				Pick_Object();
			}
		}

		if ((bNavMeshEditMode || bLightEditMode) && nullptr != pNavMeshEditorTool)
		{
			pNavMeshEditorTool->Render_Overlay(vImagePos, m_iRTWidth, m_iRTHeight);
		}
		else if (bUICanvasMode && nullptr != pUICanvasTool)
		{
			pUICanvasTool->Handle_Interaction(vImagePos, m_iRTWidth, m_iRTHeight, bViewportImageHovered);
			pUICanvasTool->Render_Overlay(vImagePos, m_iRTWidth, m_iRTHeight);
		}
	}

	ImGui::End();
}

#pragma region CAMERA
void   CPanel_Viewport::Resize_ActiveCameraProjection()
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
                CCamera* pCamera = dynamic_cast<CCamera*>(pObject);
                if (nullptr == pCamera)
                        continue;

                if (false == pCamera->Is_ActiveCamera())
                        continue;

                pCamera->Resize_Projection(m_iRTWidth, m_iRTHeight);
        }
}

#pragma endregion

#pragma region RENDER_TARGET

HRESULT CPanel_Viewport::Begin_RT()
{
	if (0 == m_iRTWidth || 0 == m_iRTHeight)
		return E_FAIL;

	return m_pGameInstance->Begin_ViewportRT(m_iRTWidth, m_iRTHeight);
}

HRESULT CPanel_Viewport::End_RT()
{
	return m_pGameInstance->End_ViewportRT();
}

ID3D11ShaderResourceView* CPanel_Viewport::Get_SRV() const
{
	return m_pGameInstance->Get_ViewportSRV();
}

#pragma endregion

#pragma region PICKING

void CPanel_Viewport::Pick_Object()
{
	PICK_RESULT Result{};

	if (Pick_Surface(&Result, false) && nullptr != Result.pObject)
		m_pPanel_Manager->Set_SelectedObject(Result.pObject);
	else
		m_pPanel_Manager->Clear_Selection();
}

#pragma endregion

_bool CPanel_Viewport::Pick_Surface(PICK_RESULT* pOutResult, _bool bMapOnly)
{
	if (nullptr == pOutResult || 0 == m_iRTWidth || 0 == m_iRTHeight)
		return false;

	_float4 vRayOrigin = {};
	_float4 vRayDir = {};

	m_pGameInstance->Compute_WorldRay(
		m_fPickX, m_fPickY,
		static_cast<_float>(m_iRTWidth),
		static_cast<_float>(m_iRTHeight),
		&vRayOrigin, &vRayDir);

	_vector vOrigin = XMLoadFloat4(&vRayOrigin);
	_vector vDir = XMLoadFloat4(&vRayDir);

	_int iLevelIndex = m_pGameInstance->Get_CurrentLevelIndex();
	if (iLevelIndex < 0)
		return false;

	const auto* pLayers = m_pGameInstance->Get_Layers(static_cast<_uint>(iLevelIndex));
	if (nullptr == pLayers)
		return false;

	CGameObject* pPicked = { nullptr };
	_float fMinDist = FLT_MAX;

	for (auto& LayerPair : *pLayers)
	{
		if (LayerPair.first == TEXT("Layer_NavMesh"))
			continue;

		if (bMapOnly && LayerPair.first != TEXT("Layer_BackGround"))
			continue;

		for (auto& pObject : LayerPair.second->Get_GameObjects())
		{
			if (nullptr == pObject || nullptr == pObject->Get_Transform())
				continue;

			_float fDist = {};
			_bool bHit = false;

			_matrix matWorld = XMLoadFloat4x4(pObject->Get_Transform()->Get_WorldMatrixPtr());

			CVIBuffer* pVIBuffer = pObject->Get_VIBuffer();
			if (nullptr != pVIBuffer)
			{
				bHit = pVIBuffer->Pick(vOrigin, vDir, matWorld, fDist);
			}
			else
			{
				auto& Components = pObject->Get_Components();
				auto iter = Components.find(TEXT("Com_Model"));
				if (iter != Components.end())
				{
					CModel* pModel = static_cast<CModel*>(iter->second);
					bHit = pModel->Pick(vOrigin, vDir, matWorld, fDist);
				}
			}

			if (bHit && fDist < fMinDist)
			{
				fMinDist = fDist;
				pPicked = pObject;
			}

			CContainerObject* pContainer = dynamic_cast<CContainerObject*>(pObject);
			if (nullptr == pContainer)
				continue;

			for (auto& PartPair : pContainer->Get_PartObjects())
			{
				CPartObject* pPartObject = PartPair.second;
				if (nullptr == pPartObject)
					continue;

				auto& PartComponents = pPartObject->Get_Components();
				auto itModel = PartComponents.find(TEXT("Com_Model"));
				if (itModel == PartComponents.end())
					continue;

				CModel* pModel = static_cast<CModel*>(itModel->second);
				if (nullptr == pModel)
					continue;

				_matrix matPartWorld = XMLoadFloat4x4(&pPartObject->Get_CombinedWorldMatrix());

				_float fPartDist = {};
				if (pModel->Pick(vOrigin, vDir, matPartWorld, fPartDist))
				{
					if (fPartDist < fMinDist)
					{
						fMinDist = fPartDist;
						pPicked = pPartObject;
					}
				}
			}
		}
	}

	if (nullptr == pPicked)
		return false;

	_vector vHitPosition = vOrigin + XMVector3Normalize(vDir) * fMinDist;

	pOutResult->pObject = pPicked;
	pOutResult->fDistance = fMinDist;
	XMStoreFloat3(&pOutResult->vPosition, vHitPosition);

	return true;
}



CUICanvasTool* CPanel_Viewport::Find_UICanvasTool()
{
	if (nullptr == m_pPanel_Manager)
		return nullptr;

	CPanel* pPanel = m_pPanel_Manager->Get_Panel(TEXT("Panel_2DCanvas"));
	CPanel_2DCanvas* pCanvas = dynamic_cast<CPanel_2DCanvas*>(pPanel);
	if (nullptr == pCanvas)
		return nullptr;

	return pCanvas->Get_Tool();
}

CPanel_Viewport* CPanel_Viewport::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CPanel_Viewport* pInstance = new CPanel_Viewport(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Create : CPanel_Viewport");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CPanel_Viewport::Free()
{
	__super::Free();
}
