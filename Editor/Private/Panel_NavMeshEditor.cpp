#include "Panel_NavMeshEditor.h"
#include "Panel_Manager.h"
#include "NavMeshEditorTool.h"

namespace
{
	const _char* Get_SpawnTypeLabel(SPAWN_TYPE eType)
	{
		switch (eType)
		{
		case SPAWN_TYPE::PLAYER:
			return "Player";
		case SPAWN_TYPE::MONSTER_NORMAL:
			return "Normal";
		case SPAWN_TYPE::MONSTER_ELITE:
			return "Elite";
		case SPAWN_TYPE::MONSTER_BOSS:
			return "Boss";
		default:
			return "Unknown";
		}
	}

	const _char* Get_LightTypeLabel(LIGHT eType)
	{
		switch (eType)
		{
		case LIGHT::DIRECTIONAL:
			return "Directional";
		case LIGHT::POINT:
			return "Point";
		default:
			return "Unknown";
		}
	}
}

CPanel_NavMeshEditor::CPanel_NavMeshEditor(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPanel{ pDevice, pContext }
{
}

HRESULT CPanel_NavMeshEditor::Initialize()
{
	strcpy_s(m_szName, "NavMesh Editor");

	m_pTool = CNavMeshEditorTool::Create();
	if (nullptr == m_pTool)
		return E_FAIL;

	return S_OK;
}

void CPanel_NavMeshEditor::Update(_float fTimeDelta)
{
}

void CPanel_NavMeshEditor::Render()
{
	ImGui::Begin(m_szName, &m_bOpen);

	if (nullptr == m_pTool)
	{
		ImGui::TextDisabled("NavMesh editor tool not found.");
		ImGui::End();
		return;
	}

	_bool bNavMeshMode = m_pPanel_Manager->Is_NavMeshEditMode();
	if (ImGui::Checkbox("NavMesh Edit Mode", &bNavMeshMode))
	{
		m_pPanel_Manager->Set_ToolMode(
			bNavMeshMode ? EDITOR_TOOL_MODE::NAVMESH : EDITOR_TOOL_MODE::OBJECT);
	}

	_bool bLightMode = m_pPanel_Manager->Is_LightEditMode();
	if (ImGui::Checkbox("Light Edit Mode", &bLightMode))
	{
		m_pPanel_Manager->Set_ToolMode(
			bLightMode ? EDITOR_TOOL_MODE::LIGHT : EDITOR_TOOL_MODE::OBJECT);
	}

	ImGui::Separator();

	if (ImGui::Button("Clear Picks"))
		m_pTool->Clear_PickPoints();

	ImGui::SameLine();

	const _bool bCreateDisabled = m_pTool->Get_NumPickPoints() < 3;
	if (bCreateDisabled)
		ImGui::BeginDisabled();

	if (ImGui::Button("Create Cell"))
		m_pTool->Create_NavMeshCell();

	if (bCreateDisabled)
		ImGui::EndDisabled();

	const _bool bNoCell = INVALID_INDEX == m_pTool->Get_SelectedCellIndex();

	if (bNoCell)
		ImGui::BeginDisabled();

	if (ImGui::Button("Delete Cell"))
		m_pTool->Delete_SelectedCell();

	if (bNoCell)
		ImGui::EndDisabled();

	const _bool bUndoDisabled = false == m_pTool->Can_Undo();
	const _bool bRedoDisabled = false == m_pTool->Can_Redo();

	if (bUndoDisabled)
		ImGui::BeginDisabled();

	if (ImGui::Button("Undo"))
		m_pTool->Undo();

	if (bUndoDisabled)
		ImGui::EndDisabled();

	ImGui::SameLine();

	if (bRedoDisabled)
		ImGui::BeginDisabled();

	if (ImGui::Button("Redo"))
		m_pTool->Redo();

	if (bRedoDisabled)
		ImGui::EndDisabled();

	ImGui::Separator();

	if (ImGui::Button("Save NavData"))
		m_pTool->Save_NavData();

	ImGui::SameLine();

	if (ImGui::Button("Load NavData"))
		m_pTool->Load_NavData();

	ImGui::Separator();

	if (bNoCell)
		ImGui::BeginDisabled();

	if (ImGui::Button("Set Player Spawn"))
		m_pTool->Set_PlayerSpawnPoint();

	if (ImGui::Button("Add Normal Spawn"))
		m_pTool->Add_MonsterSpawnPoint(SPAWN_TYPE::MONSTER_NORMAL);

	ImGui::SameLine();

	if (ImGui::Button("Add Elite Spawn"))
		m_pTool->Add_MonsterSpawnPoint(SPAWN_TYPE::MONSTER_ELITE);

	ImGui::SameLine();

	if (ImGui::Button("Add Boss Spawn"))
		m_pTool->Add_MonsterSpawnPoint(SPAWN_TYPE::MONSTER_BOSS);

	if (bNoCell)
		ImGui::EndDisabled();

	const _uint iNumSpawnPoints = m_pTool->Get_NumSpawnPoints();
	if (0 < iNumSpawnPoints)
	{
		ImGui::TextDisabled("SpawnPoint List");

		ImGui::BeginChild("SpawnPointList", ImVec2(0.f, 96.f), true);

		for (_uint i = 0; i < iNumSpawnPoints; ++i)
		{
			const SPAWN_POINT* pPoint = m_pTool->Get_SpawnPoint(i);
			if (nullptr == pPoint)
				continue;

			_char szLabel[128] = {};
			sprintf_s(
				szLabel,
				"%u. %s / Cell %d",
				i,
				Get_SpawnTypeLabel(pPoint->eType),
				pPoint->iNavCellIndex);

			if (ImGui::Selectable(szLabel, m_pTool->Get_SelectedSpawnPointIndex() == static_cast<_int>(i)))
				m_pTool->Set_SelectedSpawnPointIndex(static_cast<_int>(i));
		}

		ImGui::EndChild();
	}
	else
	{
		ImGui::TextDisabled("No SpawnPoints.");
	}

	const _bool bNoSpawnPoint = 0 == iNumSpawnPoints;
	const _bool bNoSelectedSpawnPoint = INVALID_INDEX == m_pTool->Get_SelectedSpawnPointIndex();

	if (false == bNoSelectedSpawnPoint)
	{
		const _int iSelectedSpawnPoint = m_pTool->Get_SelectedSpawnPointIndex();

		const SPAWN_POINT* pSelectedSpawnPoint =
			m_pTool->Get_SpawnPoint(static_cast<_uint>(iSelectedSpawnPoint));

		if (nullptr != pSelectedSpawnPoint)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Selected Spawn Transform");

			_float3 vRotationDeg = pSelectedSpawnPoint->vRotationDeg;

			if (ImGui::DragFloat3("Rotation Deg", &vRotationDeg.x, 1.f, -360.f, 360.f, "%.1f"))
				m_pTool->Set_SelectedSpawnPointRotation(vRotationDeg);

			if (ImGui::Button("Yaw 0"))
				m_pTool->Set_SelectedSpawnPointYaw(0.f);

			ImGui::SameLine();

			if (ImGui::Button("Yaw 90"))
				m_pTool->Set_SelectedSpawnPointYaw(90.f);

			ImGui::SameLine();

			if (ImGui::Button("Yaw 180"))
				m_pTool->Set_SelectedSpawnPointYaw(180.f);

			ImGui::SameLine();

			if (ImGui::Button("Yaw -90"))
				m_pTool->Set_SelectedSpawnPointYaw(-90.f);

			ImGui::Separator();
			ImGui::TextDisabled("Monster Info (Lv / Name)");

			_int iLevel = pSelectedSpawnPoint->iLevel;
			if (ImGui::DragInt("Level", &iLevel, 1, 1, 999))
				m_pTool->Set_SelectedSpawnPointLevel(iLevel);

			// wchar_t 입력은 ImGui 가 char* 만 받으므로 UTF-8 ↔ UTF-16 변환
			char szDisplay_UTF8[256] = {};
			::WideCharToMultiByte(CP_UTF8, 0, pSelectedSpawnPoint->szDisplayName, -1,
				szDisplay_UTF8, sizeof(szDisplay_UTF8), nullptr, nullptr);

			if (ImGui::InputText("DisplayName", szDisplay_UTF8, sizeof(szDisplay_UTF8)))
			{
				_tchar szDisplay_W[MAX_PATH] = {};
				::MultiByteToWideChar(CP_UTF8, 0, szDisplay_UTF8, -1,
					szDisplay_W, MAX_PATH);
				m_pTool->Set_SelectedSpawnPointDisplayName(szDisplay_W);
			}
		}
	}

	if (bNoSelectedSpawnPoint)
		ImGui::BeginDisabled();

	if (ImGui::Button("Delete Selected Spawn"))
		m_pTool->Delete_SelectedSpawnPoint();

	if (bNoSelectedSpawnPoint)
		ImGui::EndDisabled();

	ImGui::SameLine();

	if (bNoSpawnPoint)
		ImGui::BeginDisabled();

	if (ImGui::Button("Clear Spawns"))
		m_pTool->Clear_SpawnPoints();

	if (bNoSpawnPoint)
		ImGui::EndDisabled();

	// Light UI 
	ImGui::Separator();
	ImGui::TextDisabled("Scene Lights");

	if (ImGui::Button("Add Directional"))
		m_pTool->Add_DefaultDirectionalLight();

	ImGui::SameLine();

	if (ImGui::Button("Delete Selected Light"))
		m_pTool->Delete_SelectedLight();

	ImGui::SameLine();

	if (ImGui::Button("Clear Lights"))
		m_pTool->Clear_Lights();

	const _uint iNumSceneLights = m_pTool->Get_NumSceneLights();

	if (0 < iNumSceneLights)
	{
		ImGui::BeginChild("SceneLightList", ImVec2(0.f, 96.f), true);

		for (_uint i = 0; i < iNumSceneLights; ++i)
		{
			const SCENE_LIGHT* pLight = m_pTool->Get_SceneLight(i);
			if (nullptr == pLight)
				continue;

			_char szLabel[128] = {};
			sprintf_s(
				szLabel,
				"%u. %s",
				i,
				Get_LightTypeLabel(pLight->eType));

			if (ImGui::Selectable(szLabel, m_pTool->Get_SelectedLightIndex() == static_cast<_int>(i)))
				m_pTool->Set_SelectedLightIndex(static_cast<_int>(i));
		}

		ImGui::EndChild();
	}
	else
	{
		ImGui::TextDisabled("No SceneLights.");
	}

	const _int iSelectedLight = m_pTool->Get_SelectedLightIndex();

	if (INVALID_INDEX != iSelectedLight)
	{
		const SCENE_LIGHT* pSelectedLight = m_pTool->Get_SceneLight(static_cast<_uint>(iSelectedLight));

		if (nullptr != pSelectedLight)
		{
			SCENE_LIGHT Light = *pSelectedLight;

			ImGui::Separator();
			ImGui::TextDisabled("Selected Light");

			if (LIGHT::POINT == Light.eType)
			{
				_float3 vPosition = _float3(Light.vPosition.x, Light.vPosition.y, Light.vPosition.z);

				if (ImGui::DragFloat3("Position", &vPosition.x, 0.1f, -1000.f, 1000.f, "%.2f"))
				{
					Light.vPosition = _float4(vPosition.x, vPosition.y, vPosition.z, 1.f);
					m_pTool->Set_SelectedSceneLight(Light);
				}

				if (ImGui::DragFloat("Range", &Light.fRange, 0.1f, 0.1f, 100.f, "%.2f"))
					m_pTool->Set_SelectedSceneLight(Light);
			}
			else if (LIGHT::DIRECTIONAL == Light.eType)
			{
				_float3 vDirection = _float3(Light.vDirection.x, Light.vDirection.y, Light.vDirection.z);

				if (ImGui::DragFloat3("Direction", &vDirection.x, 0.01f, -1.f, 1.f, "%.3f"))
				{
					_vector vDir = XMVector3Normalize(XMLoadFloat3(&vDirection));
					XMStoreFloat3(&vDirection, vDir);

					Light.vDirection = _float4(vDirection.x, vDirection.y, vDirection.z, 0.f);
					m_pTool->Set_SelectedSceneLight(Light);
				}
			}

			if (ImGui::ColorEdit4("Diffuse", &Light.vDiffuse.x))
				m_pTool->Set_SelectedSceneLight(Light);

			if (ImGui::ColorEdit4("Ambient", &Light.vAmbient.x))
				m_pTool->Set_SelectedSceneLight(Light);

			if (ImGui::ColorEdit4("Specular", &Light.vSpecular.x))
				m_pTool->Set_SelectedSceneLight(Light);
		}
	}

	if (ImGui::Button("Save SceneData"))
		m_pTool->Save_SceneData();

	ImGui::SameLine();

	if (ImGui::Button("Load SceneData"))
		m_pTool->Load_SceneData();

	ImGui::Separator();

	ImGui::Text("Selected Cell: %d", m_pTool->Get_SelectedCellIndex());
	ImGui::Text("Selected Vertex: %d", m_pTool->Get_SelectedVertexIndex());
	ImGui::Text("Pending Points: %u", m_pTool->Get_NumPickPoints());
	ImGui::Text("SpawnPoints: %u", m_pTool->Get_NumSpawnPoints());

	ImGui::End();
}

CPanel_NavMeshEditor* CPanel_NavMeshEditor::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CPanel_NavMeshEditor* pInstance = new CPanel_NavMeshEditor(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Create : CPanel_NavMeshEditor");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CPanel_NavMeshEditor::Free()
{
	Safe_Release(m_pTool);

	__super::Free();
}
