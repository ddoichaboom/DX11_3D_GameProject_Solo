#include "NavMeshEditorTool.h"
#include "GameInstance.h"
#include "Layer.h"
#include "GameObject.h"
#include "Model.h"
#include "Mesh.h"
#include "VIBuffer.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "NavMeshObject.h"
#include "NavMesh.h"
#include "Cell.h"
#include "SceneSerializer.h"
#include "UICanvasTool.h"

namespace
{
	static const _tchar* NAVDATA_PATH = TEXT("../../Resources/NavMesh/ThroneRoom.navdata");
	static const _tchar* SCENEDATA_PATH = TEXT("../../Resources/Scenes/Map/ThroneRoom.scene");

	const char* Get_SpawnTypeLabel(SPAWN_TYPE eType)
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

	ImU32 Get_SpawnColor(SPAWN_TYPE eType)
	{
		switch (eType)
		{
		case SPAWN_TYPE::PLAYER:
			return IM_COL32(80, 200, 255, 255);
		case SPAWN_TYPE::MONSTER_NORMAL:
			return IM_COL32(120, 255, 120, 255);
		case SPAWN_TYPE::MONSTER_ELITE:
			return IM_COL32(255, 210, 64, 255);
		case SPAWN_TYPE::MONSTER_BOSS:
			return IM_COL32(255, 80, 80, 255);
		default:
			return IM_COL32(255, 255, 255, 255);
		}
	}

	ImU32 Get_LightColor(LIGHT eType)
	{
		switch (eType)
		{
		case LIGHT::DIRECTIONAL:
			return IM_COL32(255, 255, 160, 255);
		case LIGHT::POINT:
			return IM_COL32(255, 180, 80, 255);
		default:
			return IM_COL32(255, 255, 255, 255);
		}
	}

	const char* Get_LightTypeLabel(LIGHT eType)
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

	void Sort_SceneLightsForAuthoring(vector<SCENE_LIGHT>& SceneLights)
	{
		vector<SCENE_LIGHT> SortedLights;
		SortedLights.reserve(SceneLights.size());

		for (const SCENE_LIGHT& Light : SceneLights)
		{
			if (LIGHT::DIRECTIONAL == Light.eType)
				SortedLights.push_back(Light);
		}

		for (const SCENE_LIGHT& Light : SceneLights)
		{
			if (LIGHT::DIRECTIONAL != Light.eType)
				SortedLights.push_back(Light);
		}

		SceneLights = SortedLights;
	}
}

CNavMeshEditorTool::CNavMeshEditorTool()
	: m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
}

void CNavMeshEditorTool::Render_Overlay(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	Render_SpawnPoints(vImagePos, iViewportWidth, iViewportHeight);
	Render_Lights(vImagePos, iViewportWidth, iViewportHeight);
	Render_SelectedCell(vImagePos, iViewportWidth, iViewportHeight);
	Render_SelectedVertex(vImagePos, iViewportWidth, iViewportHeight);
	Render_PickPreview(vImagePos, iViewportWidth, iViewportHeight);
}

void CNavMeshEditorTool::Render_CamColliderOverlay(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	Render_CamColliderAuthoring(vImagePos, iViewportWidth, iViewportHeight);
	Render_CamColliderPreview(vImagePos, iViewportWidth, iViewportHeight);
}

void CNavMeshEditorTool::Handle_ViewportClick(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	if (ImGui::IsKeyDown(ImGuiMod_Ctrl))
		Select_Cell(fPickX, fPickY, iViewportWidth, iViewportHeight);
	else if (ImGui::IsKeyDown(ImGuiMod_Alt))
		Select_Vertex(fPickX, fPickY, iViewportWidth, iViewportHeight);
	else if (ImGui::IsKeyDown(ImGuiMod_Shift))
		Move_SelectedVertex(fPickX, fPickY, iViewportWidth, iViewportHeight);
	else
		Pick_EditPoint(fPickX, fPickY, iViewportWidth, iViewportHeight);
}

void CNavMeshEditorTool::Handle_CamColliderViewportClick(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	Pick_CamColliderPoint(fPickX, fPickY, iViewportWidth, iViewportHeight);
}

void CNavMeshEditorTool::Handle_LightViewportClick(_float fPickX, _float fPickY, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	Select_Light(fPickX, fPickY, vImagePos, iViewportWidth, iViewportHeight);

	if (INVALID_INDEX != m_iSelectedLightIndex)
		return;

	Add_PointLight(fPickX, fPickY, iViewportWidth, iViewportHeight);
}

HRESULT CNavMeshEditorTool::Build_CamColliderPreview(CGameObject* pObject, _uint iMeshIndex)
{
	Clear_CamColliderPreview();

	if (nullptr == pObject)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No selected object.");
		return E_FAIL;
	}

	auto iterModel = pObject->Get_Components().find(TEXT("Com_Model"));
	if (iterModel == pObject->Get_Components().end() || nullptr == iterModel->second)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Selected object has no model.");
		return E_FAIL;
	}

	CModel* pModel = dynamic_cast<CModel*>(iterModel->second);
	if (nullptr == pModel || iMeshIndex >= pModel->Get_NumMeshes())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Invalid mesh index.");
		return E_FAIL;
	}

	CMesh* pMesh = pModel->Get_Mesh(iMeshIndex);
	if (nullptr == pMesh || nullptr == pMesh->Get_PickData())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Selected mesh has no pick data.");
		return E_FAIL;
	}

	const PICK_DATA* pPickData = pMesh->Get_PickData();
	if (nullptr == pPickData->pVerticesPos || nullptr == pPickData->pIndices || pPickData->iNumIndices < 3)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Selected mesh has no triangle data.");
		return E_FAIL;
	}

	_matrix WorldMatrix = XMMatrixIdentity();

	if (CPartObject* pPartObject = dynamic_cast<CPartObject*>(pObject))
		WorldMatrix = XMLoadFloat4x4(&pPartObject->Get_CombinedWorldMatrix());
	else if (nullptr != pObject->Get_Transform())
		WorldMatrix = XMLoadFloat4x4(pObject->Get_Transform()->Get_WorldMatrixPtr());
	else
		return E_FAIL;

	_vector vMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector vMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
	_bool bAnyVertex = false;

	for (_uint i = 0; i < pPickData->iNumVertices; ++i)
	{
		_vector vPosition = XMVector3TransformCoord(XMLoadFloat3(&pPickData->pVerticesPos[i]), WorldMatrix);

		vMin = XMVectorMin(vMin, vPosition);
		vMax = XMVectorMax(vMax, vPosition);
		bAnyVertex = true;
	}

	if (false == bAnyVertex)
	{
		Clear_CamColliderPreview();
		Log_EditStatus(LOG_LEVEL::WARNING, "Selected mesh produced no preview bounds.");
		return E_FAIL;
	}

	XMStoreFloat3(&m_vCamColliderPreviewAABBMin, vMin);
	XMStoreFloat3(&m_vCamColliderPreviewAABBMax, vMax);

	m_bHasCamColliderPreview = true;
	m_iCamColliderPreviewMeshIndex = iMeshIndex;

	_char szStatus[128] = {};
	sprintf_s(szStatus, "CamCollider AABB preview mesh %u.", iMeshIndex);
	Log_EditStatus(LOG_LEVEL::INFO, szStatus);

	return S_OK;
}

void CNavMeshEditorTool::Clear_CamColliderPreview()
{
	m_bHasCamColliderPreview = false;
	m_iCamColliderPreviewMeshIndex = 0;
	m_vCamColliderPreviewAABBMin = {};
	m_vCamColliderPreviewAABBMax = {};
}

HRESULT CNavMeshEditorTool::Create_CamColliderFace()
{
	if (m_CamColliderPickedPoints.size() != 3 &&
		m_CamColliderPickedPoints.size() != 4)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Need 3 or 4 CamCollider points.");
		return E_FAIL;
	}

	_int iVertexIndices[4] = {
		INVALID_INDEX,
		INVALID_INDEX,
		INVALID_INDEX,
		INVALID_INDEX
	};

	for (_uint i = 0; i < static_cast<_uint>(m_CamColliderPickedPoints.size()); ++i)
	{
		const CAMCOLLIDER_PICK_POINT& Point = m_CamColliderPickedPoints[i];
		iVertexIndices[i] = Point.bSnapped
			? Point.iSnapVertexIndex
			: Find_OrAddCamColliderVertex(Point.vPreviewPosition);

		if (INVALID_INDEX == iVertexIndices[i])
		{
			Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to create CamCollider vertex.");
			return E_FAIL;
		}
	}

	for (_uint i = 0; i < static_cast<_uint>(m_CamColliderPickedPoints.size()); ++i)
	{
		for (_uint j = i + 1; j < static_cast<_uint>(m_CamColliderPickedPoints.size()); ++j)
		{
			if (iVertexIndices[i] == iVertexIndices[j])
			{
				Log_EditStatus(LOG_LEVEL::WARNING, "Duplicated CamCollider vertices.");
				return E_FAIL;
			}
		}
	}

	CAMCOLLIDER_FACE Face0{};
	if (false == Build_CamColliderTriangle(iVertexIndices[0], iVertexIndices[1], iVertexIndices[2], &Face0))
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Invalid CamCollider triangle.");
		return E_FAIL;
	}

	m_CamColliderFaces.push_back(Face0);
	m_iSelectedCamColliderFaceIndex = static_cast<_int>(m_CamColliderFaces.size() - 1);

	if (4 == m_CamColliderPickedPoints.size())
	{
		CAMCOLLIDER_FACE Face1{};
		if (false == Build_CamColliderTriangle(iVertexIndices[0], iVertexIndices[2], iVertexIndices[3], &Face1))
		{
			m_CamColliderFaces.pop_back();
			m_iSelectedCamColliderFaceIndex = INVALID_INDEX;
			Log_EditStatus(LOG_LEVEL::WARNING, "Invalid CamCollider quad.");
			return E_FAIL;
		}

		m_CamColliderFaces.push_back(Face1);
		m_iSelectedCamColliderFaceIndex = static_cast<_int>(m_CamColliderFaces.size() - 1);
	}

	Clear_CamColliderPickPoints();

	_char szStatus[128] = {};
	sprintf_s(szStatus, "Created CamCollider Face. Faces: %u", static_cast<_uint>(m_CamColliderFaces.size()));
	Log_EditStatus(LOG_LEVEL::INFO, szStatus);

	return S_OK;
}

void CNavMeshEditorTool::Clear_CamColliderPickPoints()
{
	m_CamColliderPickedPoints.clear();
}

HRESULT CNavMeshEditorTool::Delete_SelectedCamColliderFace()
{
	if (m_iSelectedCamColliderFaceIndex < 0 ||
		static_cast<size_t>(m_iSelectedCamColliderFaceIndex) >= m_CamColliderFaces.size())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No selected CamCollider face.");
		return E_FAIL;
	}

	m_CamColliderFaces.erase(m_CamColliderFaces.begin() + m_iSelectedCamColliderFaceIndex);

	if (m_CamColliderFaces.empty())
		m_iSelectedCamColliderFaceIndex = INVALID_INDEX;
	else if (static_cast<size_t>(m_iSelectedCamColliderFaceIndex) >= m_CamColliderFaces.size())
		m_iSelectedCamColliderFaceIndex = static_cast<_int>(m_CamColliderFaces.size() - 1);

	Log_EditStatus(LOG_LEVEL::INFO, "Deleted selected CamCollider face.");

	return S_OK;
}

void CNavMeshEditorTool::Clear_CamColliders()
{
	m_CamColliderPickedPoints.clear();
	m_CamColliderVertices.clear();
	m_CamColliderFaces.clear();
	m_iSelectedCamColliderFaceIndex = INVALID_INDEX;

	Log_EditStatus(LOG_LEVEL::INFO, "Cleared CamColliders.");
}

void CNavMeshEditorTool::Flip_SelectedCamColliderFace()
{
	if (m_iSelectedCamColliderFaceIndex < 0 ||
		static_cast<size_t>(m_iSelectedCamColliderFaceIndex) >= m_CamColliderFaces.size())
		return;

	CAMCOLLIDER_FACE& Face = m_CamColliderFaces[m_iSelectedCamColliderFaceIndex];
	std::swap(Face.iVertexIndices[1], Face.iVertexIndices[2]);
	Face.vNormal.x *= -1.f;
	Face.vNormal.y *= -1.f;
	Face.vNormal.z *= -1.f;
}

HRESULT CNavMeshEditorTool::Create_NavMeshCell()
{
	if (3 != m_NavMeshPickedPoints.size())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Need exactly 3 points.");
		return E_FAIL;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return E_FAIL;
	}

	NAVMESH_SNAPSHOT Backup = pNavMesh->Capture_Snapshot();

	_int iVertexIndices[3] = {
		INVALID_INDEX,
		INVALID_INDEX,
		INVALID_INDEX
	};

	for (_uint i = 0; i < 3; ++i)
	{
		const NAVMESH_PICK_POINT& Point = m_NavMeshPickedPoints[i];

		if (Point.bSnapped)
			iVertexIndices[i] = Point.iSnapVertexIndex;
		else
			iVertexIndices[i] = pNavMesh->Find_OrAddVertex(Point.vPreviewPosition);

		if (INVALID_INDEX == iVertexIndices[i])
		{
			pNavMesh->Restore_Snapshot(Backup);
			Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to create vertex.");
			return E_FAIL;
		}
	}

	if (iVertexIndices[0] == iVertexIndices[1] ||
		iVertexIndices[1] == iVertexIndices[2] ||
		iVertexIndices[2] == iVertexIndices[0])
	{
		pNavMesh->Restore_Snapshot(Backup);
		Log_EditStatus(LOG_LEVEL::WARNING, "Duplicated vertices.");
		return E_FAIL;
	}

	_int iCellIndex = INVALID_INDEX;
	if (FAILED(pNavMesh->Try_AddCell(
		iVertexIndices[0],
		iVertexIndices[1],
		iVertexIndices[2],
		&iCellIndex)))
	{
		pNavMesh->Restore_Snapshot(Backup);
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to create cell.");
		return E_FAIL;
	}

	Push_UndoSnapshot(Backup);
	m_iSelectedNavMeshCellIndex = iCellIndex;
	Clear_PickPoints();

	_char szStatus[128] = {};
	sprintf_s(szStatus, "Created Cell: %d", iCellIndex);
	Log_EditStatus(LOG_LEVEL::INFO, szStatus);

	return S_OK;
}

void CNavMeshEditorTool::Clear_PickPoints()
{
	m_NavMeshPickedPoints.clear();
	m_bHasLastNavMeshPick = false;
	m_vLastNavMeshPick = {};
}

HRESULT CNavMeshEditorTool::Delete_SelectedCell()
{
	if (INVALID_INDEX == m_iSelectedNavMeshCellIndex)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No selected cell.");
		return E_FAIL;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return E_FAIL;
	}

	NAVMESH_SNAPSHOT Backup = pNavMesh->Capture_Snapshot();

	if (FAILED(pNavMesh->Remove_Cell(m_iSelectedNavMeshCellIndex)))
	{
		pNavMesh->Restore_Snapshot(Backup);
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to delete cell.");
		return E_FAIL;
	}

	Push_UndoSnapshot(Backup);
	m_iSelectedNavMeshCellIndex = INVALID_INDEX;
	Clear_PickPoints();

	Log_EditStatus(LOG_LEVEL::INFO, "Deleted selected cell.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Undo()
{
	if (m_NavMeshUndoStack.empty())
		return E_FAIL;

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
		return E_FAIL;

	NAVMESH_SNAPSHOT Current = pNavMesh->Capture_Snapshot();
	NAVMESH_SNAPSHOT Previous = m_NavMeshUndoStack.back();
	m_NavMeshUndoStack.pop_back();

	if (FAILED(pNavMesh->Restore_Snapshot(Previous)))
	{
		pNavMesh->Restore_Snapshot(Current);
		Log_EditStatus(LOG_LEVEL::ERROR_, "Undo failed.");
		return E_FAIL;
	}

	m_NavMeshRedoStack.push_back(Current);
	Clear_EditState();

	Log_EditStatus(LOG_LEVEL::INFO, "Undo.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Redo()
{
	if (m_NavMeshRedoStack.empty())
		return E_FAIL;

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
		return E_FAIL;

	NAVMESH_SNAPSHOT Current = pNavMesh->Capture_Snapshot();
	NAVMESH_SNAPSHOT Next = m_NavMeshRedoStack.back();
	m_NavMeshRedoStack.pop_back();

	if (FAILED(pNavMesh->Restore_Snapshot(Next)))
	{
		pNavMesh->Restore_Snapshot(Current);
		Log_EditStatus(LOG_LEVEL::ERROR_, "Redo failed.");
		return E_FAIL;
	}

	m_NavMeshUndoStack.push_back(Current);
	Clear_EditState();

	Log_EditStatus(LOG_LEVEL::INFO, "Redo.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Save_NavData()
{
	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return E_FAIL;
	}

	_tchar szNavDataPath[MAX_PATH] = {};
	if (false == Get_CurrentNavDataPath(szNavDataPath, MAX_PATH))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Invalid NavData path.");
		return E_FAIL;
	}

	std::error_code ErrorCode{};
	std::filesystem::path NavDataPath(szNavDataPath);
	std::filesystem::create_directories(NavDataPath.parent_path(), ErrorCode);

	if (ErrorCode)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to create NavMesh directory.");
		return E_FAIL;
	}

	if (FAILED(pNavMesh->Save_NavData(szNavDataPath)))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to save NavData.");
		return E_FAIL;
	}

	Log_EditStatus(LOG_LEVEL::INFO, "Saved NavData.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Load_NavData()
{
	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return E_FAIL;
	}

	_tchar szNavDataPath[MAX_PATH] = {};
	if (false == Get_CurrentNavDataPath(szNavDataPath, MAX_PATH))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Invalid NavData path.");
		return E_FAIL;
	}

	NAVMESH_SNAPSHOT Backup = pNavMesh->Capture_Snapshot();

	if (FAILED(pNavMesh->Load_NavData(szNavDataPath)))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to load NavData.");
		return E_FAIL;
	}

	Push_UndoSnapshot(Backup);
	Clear_EditState();

	Log_EditStatus(LOG_LEVEL::INFO, "Loaded NavData.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Set_PlayerSpawnPoint()
{
	SPAWN_POINT Point{};

	if (false == Build_SpawnPointFromSelectedCell(SPAWN_TYPE::PLAYER, TEXT("PlayerSpawn"), &Point))
		return E_FAIL;

	Push_OrReplacePlayerSpawnPoint(Point);

	Log_EditStatus(LOG_LEVEL::INFO, "Set Player SpawnPoint.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Add_MonsterSpawnPoint(SPAWN_TYPE eType)
{
	_uint iSameTypeCount = {};

	for (const SPAWN_POINT& Point : m_SpawnPoints)
	{
		if (Point.eType == eType)
			++iSameTypeCount;
	}

	_tchar szName[MAX_PATH] = {};
	swprintf_s(szName, TEXT("%S_%02u"), Get_SpawnTypeLabel(eType), iSameTypeCount);

	SPAWN_POINT Point{};

	if (false == Build_SpawnPointFromSelectedCell(eType, szName, &Point))
		return E_FAIL;

	m_SpawnPoints.push_back(Point);

	Log_EditStatus(LOG_LEVEL::INFO, "Added Monster SpawnPoint.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Add_DefaultDirectionalLight()
{
	SCENE_LIGHT Light{};
	Light.eType = LIGHT::DIRECTIONAL;
	wcscpy_s(Light.szName, TEXT("Directional_Main"));
	Light.vDiffuse = _float4(1.f, 1.f, 1.f, 1.f);
	Light.vAmbient = _float4(0.20f, 0.22f, 0.25f, 1.f);
	Light.vSpecular = _float4(0.25f, 0.25f, 0.25f, 1.f);
	Light.vDirection = _float4(-0.5648625f, -0.8191520f, -0.0996005f, 0.f);

	m_SceneLights.push_back(Light);
	m_iSelectedLightIndex = static_cast<_int>(m_SceneLights.size() - 1);

	Log_EditStatus(LOG_LEVEL::INFO, "Added Directional Light.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Delete_SelectedLight()
{
	if (m_iSelectedLightIndex < 0 ||
		static_cast<_uint>(m_iSelectedLightIndex) >= static_cast<_uint>(m_SceneLights.size()))
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No Light selected.");
		return E_FAIL;
	}

	m_SceneLights.erase(m_SceneLights.begin() + m_iSelectedLightIndex);

	if (m_SceneLights.empty())
		m_iSelectedLightIndex = INVALID_INDEX;
	else if (static_cast<_uint>(m_iSelectedLightIndex) >= static_cast<_uint>(m_SceneLights.size()))
		m_iSelectedLightIndex = static_cast<_int>(m_SceneLights.size() - 1);

	Log_EditStatus(LOG_LEVEL::INFO, "Deleted selected Light.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Delete_LastSpawnPoint()
{
	if (m_SpawnPoints.empty())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "SpawnPoint does not exist.");
		return E_FAIL;
	}

	m_SpawnPoints.pop_back();
	if (static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
	{
		m_iSelectedSpawnPointIndex = m_SpawnPoints.empty()
			? INVALID_INDEX
			: static_cast<_int>(m_SpawnPoints.size() - 1);
	}

	Log_EditStatus(LOG_LEVEL::INFO, "Deleted last SpawnPoint.");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Delete_SelectedSpawnPoint()
{
	if (m_iSelectedSpawnPointIndex < 0 ||
		static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No SpawnPoint selected.");
		return E_FAIL;
	}

	m_SpawnPoints.erase(m_SpawnPoints.begin() + m_iSelectedSpawnPointIndex);

	if (m_SpawnPoints.empty())
		m_iSelectedSpawnPointIndex = INVALID_INDEX;
	else if (static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
		m_iSelectedSpawnPointIndex = static_cast<_int>(m_SpawnPoints.size() - 1);

	Log_EditStatus(LOG_LEVEL::INFO, "Deleted selected SpawnPoint.");

	return S_OK;
}

void CNavMeshEditorTool::Clear_SpawnPoints()
{
	if (m_SpawnPoints.empty())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "SpawnPoint does not exist.");
		return;
	}

	m_SpawnPoints.clear();
	m_iSelectedSpawnPointIndex = INVALID_INDEX;

	Log_EditStatus(LOG_LEVEL::INFO, "Cleared SpawnPoints.");
}

void CNavMeshEditorTool::Clear_Lights()
{
	if (m_SceneLights.empty())
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "Light does not exist.");
		return;
	}

	m_SceneLights.clear();
	m_iSelectedLightIndex = INVALID_INDEX;

	Log_EditStatus(LOG_LEVEL::INFO, "Cleared Lights.");
}

HRESULT CNavMeshEditorTool::Save_SceneData()
{
	std::error_code ErrorCode{};
	std::filesystem::create_directories(std::filesystem::path(TEXT("../../Resources/Scenes/Map")), ErrorCode);

	if (ErrorCode)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to create Scenes directory.");
		return E_FAIL;
	}

	SCENE_DATA SceneData{};
	SCENE_DATA ExistingSceneData{};
	const _bool bExistingSceneLoaded = SUCCEEDED(CSceneSerializer::Load(SCENEDATA_PATH, &ExistingSceneData));

	if (false == Get_CurrentNavDataPath(SceneData.szNavDataPath, MAX_PATH))
		wcscpy_s(SceneData.szNavDataPath, NAVDATA_PATH);

	SceneData.SpawnPoints = m_SpawnPoints;
	SceneData.SceneLights = m_SceneLights;
	SceneData.CamColliderVertices = m_CamColliderVertices;
	SceneData.CamColliderFaces = m_CamColliderFaces;
	Sort_SceneLightsForAuthoring(SceneData.SceneLights);

	if (m_SpawnPoints.empty())
	{
		if (true == bExistingSceneLoaded &&
			false == ExistingSceneData.SpawnPoints.empty())
		{
			SceneData.SpawnPoints = ExistingSceneData.SpawnPoints;
			Log_EditStatus(LOG_LEVEL::INFO, "Preserved existing SpawnPoints.");
		}
	}

	if (m_CamColliderVertices.empty() &&
		m_CamColliderFaces.empty())
	{
		if (true == bExistingSceneLoaded &&
			(false == ExistingSceneData.CamColliderVertices.empty() ||
				false == ExistingSceneData.CamColliderFaces.empty()))
		{
			SceneData.CamColliderVertices = ExistingSceneData.CamColliderVertices;
			SceneData.CamColliderFaces = ExistingSceneData.CamColliderFaces;
			Log_EditStatus(LOG_LEVEL::INFO, "Preserved existing CamColliders.");
		}
	}

	if (FAILED(CSceneSerializer::Save(SCENEDATA_PATH, SceneData)))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to save SceneData.");
		return E_FAIL;
	}

	Log_EditStatus(LOG_LEVEL::INFO, "Saved SceneData: ../../Resources/Scenes/Map/ThroneRoom.scene");

	return S_OK;
}

HRESULT CNavMeshEditorTool::Load_SceneData()
{
	SCENE_DATA SceneData{};

	if (FAILED(CSceneSerializer::Load(SCENEDATA_PATH, &SceneData)))
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to load SceneData.");
		return E_FAIL;
	}

	m_SpawnPoints = SceneData.SpawnPoints;
	m_SceneLights = SceneData.SceneLights;
	m_CamColliderVertices = SceneData.CamColliderVertices;
	m_CamColliderFaces = SceneData.CamColliderFaces;
	Sort_SceneLightsForAuthoring(m_SceneLights);

	m_iSelectedSpawnPointIndex = INVALID_INDEX;
	m_iSelectedLightIndex = INVALID_INDEX;
	m_iSelectedCamColliderFaceIndex = INVALID_INDEX;
	m_CamColliderPickedPoints.clear();

	Log_EditStatus(LOG_LEVEL::INFO, "Loaded SceneData: ../../Resources/Scenes/Map/ThroneRoom.scene");

	return S_OK;
}

const SPAWN_POINT* CNavMeshEditorTool::Get_SpawnPoint(_uint iIndex) const
{
	if (iIndex >= static_cast<_uint>(m_SpawnPoints.size()))
		return nullptr;

	return &m_SpawnPoints[iIndex];
}

const SCENE_LIGHT* CNavMeshEditorTool::Get_SceneLight(_uint iIndex) const
{
	if (iIndex >= static_cast<_uint>(m_SceneLights.size()))
		return nullptr;

	return &m_SceneLights[iIndex];
}

const CAMCOLLIDER_FACE* CNavMeshEditorTool::Get_CamColliderFace(_uint iIndex) const
{
	if (iIndex >= static_cast<_uint>(m_CamColliderFaces.size()))
		return nullptr;

	return &m_CamColliderFaces[iIndex];
}

void CNavMeshEditorTool::Set_SelectedCamColliderFaceIndex(_int iIndex)
{
	if (iIndex < 0 ||
		static_cast<_uint>(iIndex) >= static_cast<_uint>(m_CamColliderFaces.size()))
	{
		m_iSelectedCamColliderFaceIndex = INVALID_INDEX;
		return;
	}

	m_iSelectedCamColliderFaceIndex = iIndex;
}

void CNavMeshEditorTool::Set_SelectedLightIndex(_int iIndex)
{
	if (iIndex < 0 ||
		static_cast<_uint>(iIndex) >= static_cast<_uint>(m_SceneLights.size()))
	{
		m_iSelectedLightIndex = INVALID_INDEX;
		return;
	}

	m_iSelectedLightIndex = iIndex;
}

void CNavMeshEditorTool::Set_SelectedSceneLight(const SCENE_LIGHT& Light)
{
	if (m_iSelectedLightIndex < 0 ||
		static_cast<_uint>(m_iSelectedLightIndex) >= static_cast<_uint>(m_SceneLights.size()))
		return;

	m_SceneLights[m_iSelectedLightIndex] = Light;
}

void CNavMeshEditorTool::Set_SelectedSpawnPointIndex(_int iIndex)
{
	if (iIndex < 0 ||
		static_cast<_uint>(iIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
	{
		m_iSelectedSpawnPointIndex = INVALID_INDEX;
		return;
	}

	m_iSelectedSpawnPointIndex = iIndex;
}

void CNavMeshEditorTool::Set_SelectedSpawnPointRotation(const _float3& vRotationDeg)
{
	if (m_iSelectedSpawnPointIndex < 0 ||
		static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
		return;

	m_SpawnPoints[m_iSelectedSpawnPointIndex].vRotationDeg = vRotationDeg;
}

void CNavMeshEditorTool::Set_SelectedSpawnPointYaw(_float fYawDeg)
{
	if (m_iSelectedSpawnPointIndex < 0 ||
		static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
		return;

	m_SpawnPoints[m_iSelectedSpawnPointIndex].vRotationDeg.y = fYawDeg;
}

void CNavMeshEditorTool::Set_SelectedSpawnPointLevel(_int iLevel)
{
	if (m_iSelectedSpawnPointIndex < 0 ||
		static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
		return;

	if (iLevel < 1) iLevel = 1;
	m_SpawnPoints[m_iSelectedSpawnPointIndex].iLevel = iLevel;
}

void CNavMeshEditorTool::Set_SelectedSpawnPointDisplayName(const _tchar* pName)
{
	if (m_iSelectedSpawnPointIndex < 0 ||
		static_cast<_uint>(m_iSelectedSpawnPointIndex) >= static_cast<_uint>(m_SpawnPoints.size()))
		return;

	SPAWN_POINT& Point = m_SpawnPoints[m_iSelectedSpawnPointIndex];
	if (nullptr == pName)
		Point.szDisplayName[0] = 0;
	else
		wcscpy_s(Point.szDisplayName, pName);
}

void CNavMeshEditorTool::Render_PickPreview(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	if (m_NavMeshPickedPoints.empty())
		return;

	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	ImVec2 FirstScreen{};
	ImVec2 PrevScreen{};

	_bool bHasFirst = false;
	_bool bHasPrev = false;
	_uint iVisibleCount = 0;

	for (_uint i = 0; i < static_cast<_uint>(m_NavMeshPickedPoints.size()); ++i)
	{
		const NAVMESH_PICK_POINT& Point = m_NavMeshPickedPoints[i];

		ImVec2 ScreenPos{};
		if (false == World_To_Viewport(Point.vPreviewPosition, vImagePos, iViewportWidth, iViewportHeight, &ScreenPos))
			continue;

		const ImU32 Color = Point.bSnapped
			? IM_COL32(255, 210, 64, 255)
			: IM_COL32(64, 180, 255, 255);

		pDrawList->AddCircleFilled(ScreenPos, 5.f, Color, 16);
		pDrawList->AddCircle(ScreenPos, 9.f, Color, 16, 2.f);

		if (false == bHasFirst)
		{
			FirstScreen = ScreenPos;
			bHasFirst = true;
		}

		if (bHasPrev)
			pDrawList->AddLine(PrevScreen, ScreenPos, IM_COL32(255, 255, 255, 220), 2.f);

		PrevScreen = ScreenPos;
		bHasPrev = true;
		++iVisibleCount;
	}

	if (3 == m_NavMeshPickedPoints.size() &&
		3 == iVisibleCount &&
		bHasFirst &&
		bHasPrev)
	{
		pDrawList->AddLine(PrevScreen, FirstScreen, IM_COL32(255, 255, 255, 220), 2.f);
	}
}

void CNavMeshEditorTool::Render_CamColliderPreview(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	if (false == m_bHasCamColliderPreview)
		return;

	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	const ImU32 AABBColor = IM_COL32(255, 210, 64, 230);

	_float3 vMin = m_vCamColliderPreviewAABBMin;
	_float3 vMax = m_vCamColliderPreviewAABBMax;
	_float3 Corners[8] =
	{
		_float3(vMin.x, vMin.y, vMin.z),
		_float3(vMax.x, vMin.y, vMin.z),
		_float3(vMax.x, vMin.y, vMax.z),
		_float3(vMin.x, vMin.y, vMax.z),
		_float3(vMin.x, vMax.y, vMin.z),
		_float3(vMax.x, vMax.y, vMin.z),
		_float3(vMax.x, vMax.y, vMax.z),
		_float3(vMin.x, vMax.y, vMax.z),
	};

	static const _uint EdgeIndices[24] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7,
	};

	for (_uint i = 0; i < 24; i += 2)
	{
		ImVec2 vStart{};
		ImVec2 vEnd{};

		if (false == World_To_Viewport(Corners[EdgeIndices[i]], vImagePos, iViewportWidth, iViewportHeight, &vStart) ||
			false == World_To_Viewport(Corners[EdgeIndices[i + 1]], vImagePos, iViewportWidth, iViewportHeight, &vEnd))
			continue;

		pDrawList->AddLine(vStart, vEnd, AABBColor, 2.0f);
	}
}

void CNavMeshEditorTool::Render_CamColliderAuthoring(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	for (_uint i = 0; i < static_cast<_uint>(m_CamColliderFaces.size()); ++i)
	{
		const CAMCOLLIDER_FACE& Face = m_CamColliderFaces[i];
		ImVec2 Screen[3] = {};
		_bool bVisible = true;

		for (_uint j = 0; j < 3; ++j)
		{
			const _int iVertexIndex = Face.iVertexIndices[j];
			if (iVertexIndex < 0 ||
				static_cast<size_t>(iVertexIndex) >= m_CamColliderVertices.size() ||
				false == World_To_Viewport(m_CamColliderVertices[iVertexIndex], vImagePos, iViewportWidth, iViewportHeight, &Screen[j]))
			{
				bVisible = false;
				break;
			}
		}

		if (false == bVisible)
			continue;

		const _bool bSelected = static_cast<_int>(i) == m_iSelectedCamColliderFaceIndex;
		const ImU32 LineColor = bSelected ? IM_COL32(255, 180, 64, 255) : IM_COL32(64, 220, 255, 220);
		const _float fThickness = bSelected ? 3.f : 2.f;

		pDrawList->AddLine(Screen[0], Screen[1], LineColor, fThickness);
		pDrawList->AddLine(Screen[1], Screen[2], LineColor, fThickness);
		pDrawList->AddLine(Screen[2], Screen[0], LineColor, fThickness);

		if (bSelected)
		{
			_float3 vCenter = {};
			for (_uint j = 0; j < 3; ++j)
			{
				const _float3& vVertex = m_CamColliderVertices[Face.iVertexIndices[j]];
				vCenter.x += vVertex.x;
				vCenter.y += vVertex.y;
				vCenter.z += vVertex.z;
			}

			vCenter.x /= 3.f;
			vCenter.y /= 3.f;
			vCenter.z /= 3.f;

			_float3 vNormalEnd = _float3(
				vCenter.x + Face.vNormal.x,
				vCenter.y + Face.vNormal.y,
				vCenter.z + Face.vNormal.z);

			ImVec2 CenterScreen{};
			ImVec2 NormalEndScreen{};
			if (World_To_Viewport(vCenter, vImagePos, iViewportWidth, iViewportHeight, &CenterScreen) &&
				World_To_Viewport(vNormalEnd, vImagePos, iViewportWidth, iViewportHeight, &NormalEndScreen))
			{
				pDrawList->AddLine(CenterScreen, NormalEndScreen, IM_COL32(255, 80, 80, 255), 2.f);
				pDrawList->AddCircleFilled(NormalEndScreen, 4.f, IM_COL32(255, 80, 80, 255), 12);
			}
		}
	}

	for (_uint i = 0; i < static_cast<_uint>(m_CamColliderPickedPoints.size()); ++i)
	{
		const CAMCOLLIDER_PICK_POINT& Point = m_CamColliderPickedPoints[i];

		ImVec2 ScreenPos{};
		if (false == World_To_Viewport(Point.vPreviewPosition, vImagePos, iViewportWidth, iViewportHeight, &ScreenPos))
			continue;

		const ImU32 Color = Point.bSnapped
			? IM_COL32(255, 210, 64, 255)
			: IM_COL32(80, 220, 255, 255);

		pDrawList->AddCircleFilled(ScreenPos, 5.f, Color, 16);
		pDrawList->AddCircle(ScreenPos, 9.f, Color, 16, 2.f);

		if (i > 0)
		{
			ImVec2 PrevScreen{};
			if (World_To_Viewport(m_CamColliderPickedPoints[i - 1].vPreviewPosition, vImagePos, iViewportWidth, iViewportHeight, &PrevScreen))
				pDrawList->AddLine(PrevScreen, ScreenPos, IM_COL32(255, 255, 255, 220), 2.f);
		}
	}
}

void CNavMeshEditorTool::Render_SelectedCell(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	if (INVALID_INDEX == m_iSelectedNavMeshCellIndex)
		return;

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
		return;

	const vector<NAVMESH_CELL>& Cells = pNavMesh->Get_CellDescs();
	const vector<_float3>& Vertices = pNavMesh->Get_Vertices();

	if (m_iSelectedNavMeshCellIndex < 0 ||
		static_cast<size_t>(m_iSelectedNavMeshCellIndex) >= Cells.size())
		return;

	const NAVMESH_CELL& Cell = Cells[m_iSelectedNavMeshCellIndex];

	ImVec2 Screen[3]{};

	for (_uint i = 0; i < 3; ++i)
	{
		const _int iVertexIndex = Cell.iVertexIndices[i];

		if (iVertexIndex < 0 ||
			static_cast<size_t>(iVertexIndex) >= Vertices.size())
			return;

		if (false == World_To_Viewport(Vertices[iVertexIndex], vImagePos, iViewportWidth, iViewportHeight, &Screen[i]))
			return;
	}

	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	pDrawList->AddTriangleFilled(
		Screen[0], Screen[1], Screen[2],
		IM_COL32(255, 96, 64, 55));

	pDrawList->AddTriangle(
		Screen[0], Screen[1], Screen[2],
		IM_COL32(255, 96, 64, 255),
		3.f);
}

void CNavMeshEditorTool::Render_SelectedVertex(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	if (INVALID_INDEX == m_iSelectedNavMeshVertexIndex)
		return;

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
		return;

	const vector<_float3>& Vertices = pNavMesh->Get_Vertices();

	if (m_iSelectedNavMeshVertexIndex < 0 ||
		static_cast<size_t>(m_iSelectedNavMeshVertexIndex) >= Vertices.size())
		return;

	ImVec2 vScreenPosition = {};
	if (false == World_To_Viewport(Vertices[m_iSelectedNavMeshVertexIndex], vImagePos, iViewportWidth, iViewportHeight, &vScreenPosition))
		return;

	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	pDrawList->AddCircleFilled(
		vScreenPosition,
		7.f,
		IM_COL32(255, 210, 64, 255));

	pDrawList->AddCircle(
		vScreenPosition,
		10.f,
		IM_COL32(255, 255, 255, 255),
		16,
		2.f);
}

void CNavMeshEditorTool::Render_SpawnPoints(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	for (_uint i = 0; i < static_cast<_uint>(m_SpawnPoints.size()); ++i)
	{
		const SPAWN_POINT& Point = m_SpawnPoints[i];

		ImVec2 vScreenPosition{};
		if (false == World_To_Viewport(Point.vPosition, vImagePos, iViewportWidth, iViewportHeight, &vScreenPosition))
			continue;

		const ImU32 Color = Get_SpawnColor(Point.eType);
		const _bool bSelected = static_cast<_int>(i) == m_iSelectedSpawnPointIndex;

		pDrawList->AddCircleFilled(vScreenPosition, 6.f, Color, 16);
		pDrawList->AddCircle(
			vScreenPosition,
			bSelected ? 14.f : 11.f,
			bSelected ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255),
			16,
			bSelected ? 3.f : 2.f);

		_char szLabel[64] = {};
		sprintf_s(szLabel, "%s %u / Cell %d", Get_SpawnTypeLabel(Point.eType), i, Point.iNavCellIndex);

		pDrawList->AddText(
			ImVec2(vScreenPosition.x + 10.f, vScreenPosition.y - 8.f),
			Color,
			szLabel);
	}
}

void CNavMeshEditorTool::Select_Vertex(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
	{
		m_iSelectedNavMeshVertexIndex = INVALID_INDEX;
		Log_EditStatus(LOG_LEVEL::WARNING, "No map hit.");
		return;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		m_iSelectedNavMeshVertexIndex = INVALID_INDEX;
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return;
	}

	static constexpr _float fVertexPickRadius = { 0.35f };

	const _int iVertexIndex = pNavMesh->Find_Vertex(Result.vPosition, fVertexPickRadius);
	m_iSelectedNavMeshVertexIndex = iVertexIndex;

	if (INVALID_INDEX == iVertexIndex)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No vertex selected.");
		return;
	}

	_char szStatus[128] = {};
	sprintf_s(szStatus, "Selected Vertex: %d", iVertexIndex);
	Log_EditStatus(LOG_LEVEL::INFO, szStatus);
}

HRESULT CNavMeshEditorTool::Move_SelectedVertex(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	if (INVALID_INDEX == m_iSelectedNavMeshVertexIndex)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No selected vertex.");
		return E_FAIL;
	}

	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No map hit.");
		return E_FAIL;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return E_FAIL;
	}

	NAVMESH_SNAPSHOT Backup = pNavMesh->Capture_Snapshot();

	if (FAILED(pNavMesh->Move_Vertex(m_iSelectedNavMeshVertexIndex, Result.vPosition)))
	{
		pNavMesh->Restore_Snapshot(Backup);
		Log_EditStatus(LOG_LEVEL::ERROR_, "Failed to move vertex.");
		return E_FAIL;
	}

	Push_UndoSnapshot(Backup);
	Clear_PickPoints();

	_char szStatus[128] = {};
	sprintf_s(szStatus, "Moved Vertex: %d", m_iSelectedNavMeshVertexIndex);
	Log_EditStatus(LOG_LEVEL::INFO, szStatus);

	return S_OK;
}

void CNavMeshEditorTool::Select_Cell(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
	{
		m_iSelectedNavMeshCellIndex = INVALID_INDEX;
		Log_EditStatus(LOG_LEVEL::WARNING, "No map hit");
		return;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		m_iSelectedNavMeshCellIndex = INVALID_INDEX;
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return;
	}

	const _int iCellIndex = pNavMesh->Find_Cell(Result.vPosition);
	m_iSelectedNavMeshCellIndex = iCellIndex;

	if (INVALID_INDEX == iCellIndex)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No cell selected.");
	}
	else
	{
		_char szStatus[128] = {};
		sprintf_s(szStatus, "Selected Cell: %d", iCellIndex);
		Log_EditStatus(LOG_LEVEL::INFO, szStatus);
	}
}

void CNavMeshEditorTool::Pick_EditPoint(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
	{
		m_bHasLastNavMeshPick = false;
		return;
	}

	NAVMESH_PICK_POINT PickPoint{};
	PickPoint.vRawPosition = Result.vPosition;
	PickPoint.vPreviewPosition = Result.vPosition;

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr != pNavMesh)
	{
		const _int iSnapVertexIndex = pNavMesh->Find_Vertex(Result.vPosition);
		const vector<_float3>& Vertices = pNavMesh->Get_Vertices();

		if (iSnapVertexIndex >= 0 &&
			static_cast<size_t>(iSnapVertexIndex) < Vertices.size())
		{
			PickPoint.iSnapVertexIndex = iSnapVertexIndex;
			PickPoint.vPreviewPosition = Vertices[iSnapVertexIndex];
			PickPoint.bSnapped = true;
		}
	}

	m_vLastNavMeshPick = PickPoint.vPreviewPosition;
	m_bHasLastNavMeshPick = true;

	m_NavMeshPickedPoints.push_back(PickPoint);

	if (m_NavMeshPickedPoints.size() > 3)
		m_NavMeshPickedPoints.erase(m_NavMeshPickedPoints.begin());
}

void CNavMeshEditorTool::Pick_CamColliderPoint(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
		return;

	CAMCOLLIDER_PICK_POINT PickPoint{};
	PickPoint.vRawPosition = Result.vPosition;
	PickPoint.vPreviewPosition = Result.vPosition;

	const _int iSnapVertexIndex = Find_CamColliderVertex(Result.vPosition);
	if (iSnapVertexIndex >= 0 &&
		static_cast<size_t>(iSnapVertexIndex) < m_CamColliderVertices.size())
	{
		PickPoint.iSnapVertexIndex = iSnapVertexIndex;
		PickPoint.vPreviewPosition = m_CamColliderVertices[iSnapVertexIndex];
		PickPoint.bSnapped = true;
	}

	m_CamColliderPickedPoints.push_back(PickPoint);

	if (m_CamColliderPickedPoints.size() > 4)
		m_CamColliderPickedPoints.erase(m_CamColliderPickedPoints.begin());
}

_bool CNavMeshEditorTool::Build_SpawnPointFromSelectedCell(SPAWN_TYPE eType, const _tchar* pName, SPAWN_POINT* pOutPoint)
{
	if (nullptr == pOutPoint)
		return false;

	if (INVALID_INDEX == m_iSelectedNavMeshCellIndex)
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No selected cell.");
		return false;
	}

	CNavMesh* pNavMesh = Find_NavMesh();
	if (nullptr == pNavMesh)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "NavMeshObject not found.");
		return false;
	}

	const CCell* pCell = pNavMesh->Get_Cell(m_iSelectedNavMeshCellIndex);
	if (nullptr == pCell)
	{
		Log_EditStatus(LOG_LEVEL::ERROR_, "Invalid selected cell.");
		return false;
	}

	SPAWN_POINT Point{};
	Point.eType = eType;
	Point.iNavCellIndex = m_iSelectedNavMeshCellIndex;
	Point.vPosition = pCell->Get_Center();
	Point.vPosition.y = pNavMesh->Compute_Height(m_iSelectedNavMeshCellIndex, Point.vPosition);
	Point.vRotationDeg = _float3(0.f, 0.f, 0.f);

	if (nullptr != pName)
		wcscpy_s(Point.szName, pName);

	*pOutPoint = Point;

	return true;
}

void CNavMeshEditorTool::Push_OrReplacePlayerSpawnPoint(const SPAWN_POINT& Point)
{
	for (SPAWN_POINT& SpawnPoint : m_SpawnPoints)
	{
		if (SPAWN_TYPE::PLAYER == SpawnPoint.eType)
		{
			SpawnPoint = Point;
			return;
		}
	}

	m_SpawnPoints.push_back(Point);
}

void CNavMeshEditorTool::Push_UndoSnapshot(const NAVMESH_SNAPSHOT& Snapshot)
{
	m_NavMeshUndoStack.push_back(Snapshot);
	m_NavMeshRedoStack.clear();

	static constexpr size_t iMaxUndoCount = 64;
	if (m_NavMeshUndoStack.size() > iMaxUndoCount)
		m_NavMeshUndoStack.erase(m_NavMeshUndoStack.begin());
}

void CNavMeshEditorTool::Clear_EditState()
{
	Clear_PickPoints();
	Clear_CamColliderPickPoints();
	m_iSelectedNavMeshCellIndex = INVALID_INDEX;
	m_iSelectedNavMeshVertexIndex = INVALID_INDEX;
}

_int CNavMeshEditorTool::Find_CamColliderVertex(const _float3& vPosition, _float fSnapRadius) const
{
	const _float fSnapRadiusSq = fSnapRadius * fSnapRadius;

	for (_uint i = 0; i < static_cast<_uint>(m_CamColliderVertices.size()); ++i)
	{
		const _float3& vVertex = m_CamColliderVertices[i];

		const _float fDistanceSq =
			(vVertex.x - vPosition.x) * (vVertex.x - vPosition.x) +
			(vVertex.y - vPosition.y) * (vVertex.y - vPosition.y) +
			(vVertex.z - vPosition.z) * (vVertex.z - vPosition.z);

		if (fDistanceSq <= fSnapRadiusSq)
			return static_cast<_int>(i);
	}

	return INVALID_INDEX;
}

_int CNavMeshEditorTool::Find_OrAddCamColliderVertex(const _float3& vPosition, _float fSnapRadius)
{
	const _int iVertexIndex = Find_CamColliderVertex(vPosition, fSnapRadius);
	if (INVALID_INDEX != iVertexIndex)
		return iVertexIndex;

	m_CamColliderVertices.push_back(vPosition);
	return static_cast<_int>(m_CamColliderVertices.size() - 1);
}

_bool CNavMeshEditorTool::Build_CamColliderTriangle(_int iVertex0, _int iVertex1, _int iVertex2, CAMCOLLIDER_FACE* pOutFace) const
{
	if (nullptr == pOutFace)
		return false;

	if (iVertex0 < 0 || iVertex1 < 0 || iVertex2 < 0 ||
		static_cast<size_t>(iVertex0) >= m_CamColliderVertices.size() ||
		static_cast<size_t>(iVertex1) >= m_CamColliderVertices.size() ||
		static_cast<size_t>(iVertex2) >= m_CamColliderVertices.size() ||
		iVertex0 == iVertex1 ||
		iVertex1 == iVertex2 ||
		iVertex2 == iVertex0)
		return false;

	_vector v0 = XMLoadFloat3(&m_CamColliderVertices[iVertex0]);
	_vector v1 = XMLoadFloat3(&m_CamColliderVertices[iVertex1]);
	_vector v2 = XMLoadFloat3(&m_CamColliderVertices[iVertex2]);
	_vector vNormal = XMVector3Cross(v1 - v0, v2 - v0);

	const _float fLengthSq = XMVectorGetX(XMVector3LengthSq(vNormal));
	if (fLengthSq <= NAVMESH_MIN_CELL_AREA)
		return false;

	CAMCOLLIDER_FACE Face{};
	Face.iVertexIndices[0] = iVertex0;
	Face.iVertexIndices[1] = iVertex1;
	Face.iVertexIndices[2] = iVertex2;
	XMStoreFloat3(&Face.vNormal, XMVector3Normalize(vNormal));

	*pOutFace = Face;
	return true;
}

_bool CNavMeshEditorTool::World_To_Viewport(const _float3& vWorldPosition, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight, ImVec2* pOutScreenPosition) const
{
	if (nullptr == pOutScreenPosition || 0 == iViewportWidth || 0 == iViewportHeight)
		return false;

	const _float4x4* pViewMatrix = m_pGameInstance->Get_Transform(D3DTS::VIEW);
	const _float4x4* pProjMatrix = m_pGameInstance->Get_Transform(D3DTS::PROJ);
	if (nullptr == pViewMatrix || nullptr == pProjMatrix)
		return false;

	_matrix matView = XMLoadFloat4x4(pViewMatrix);
	_matrix matProj = XMLoadFloat4x4(pProjMatrix);

	_vector vClip = XMVector3TransformCoord(XMLoadFloat3(&vWorldPosition), matView * matProj);

	const _float fX = XMVectorGetX(vClip);
	const _float fY = XMVectorGetY(vClip);
	const _float fZ = XMVectorGetZ(vClip);

	if (fZ < 0.f || fZ > 1.f)
		return false;

	pOutScreenPosition->x = vImagePos.x + (fX + 1.f) * 0.5f * static_cast<_float>(iViewportWidth);
	pOutScreenPosition->y = vImagePos.y + (1.f - fY) * 0.5f * static_cast<_float>(iViewportHeight);

	return true;
}

_bool CNavMeshEditorTool::Pick_Surface(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight, PICK_RESULT* pOutResult, _bool bMapOnly)
{
	if (nullptr == pOutResult || 0 == iViewportWidth || 0 == iViewportHeight)
		return false;

	_float4 vRayOrigin = {};
	_float4 vRayDir = {};

	m_pGameInstance->Compute_WorldRay(
		fPickX, fPickY,
		static_cast<_float>(iViewportWidth),
		static_cast<_float>(iViewportHeight),
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

CNavMesh* CNavMeshEditorTool::Find_NavMesh() const
{
	_int iLevelIndex = m_pGameInstance->Get_CurrentLevelIndex();
	if (iLevelIndex < 0)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(static_cast<_uint>(iLevelIndex));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_NavMesh"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	for (auto& pObject : iterLayer->second->Get_GameObjects())
	{
		CNavMeshObject* pNavMeshObject = dynamic_cast<CNavMeshObject*>(pObject);
		if (nullptr == pNavMeshObject)
			continue;

		return pNavMeshObject->Get_NavMesh();
	}

	return nullptr;
}

_bool CNavMeshEditorTool::Get_CurrentNavDataPath(_tchar* pOutPath, size_t iLength) const
{
	if (nullptr == pOutPath || 0 == iLength)
		return false;

	pOutPath[0] = 0;

	SCENE_DATA SceneData{};

	if (SUCCEEDED(CSceneSerializer::Load(SCENEDATA_PATH, &SceneData)) &&
		0 != SceneData.szNavDataPath[0])
	{
		wcscpy_s(pOutPath, iLength, SceneData.szNavDataPath);
		return true;
	}

	wcscpy_s(pOutPath, iLength, NAVDATA_PATH);
	return true;
}

void CNavMeshEditorTool::Log_EditStatus(LOG_LEVEL eLevel, const string& strMessage) const
{
	Log_Message(eLevel, "[NavMesh] " + strMessage);
}

void CNavMeshEditorTool::Render_Lights(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	ImDrawList* pDrawList = ImGui::GetWindowDrawList();
	if (nullptr == pDrawList)
		return;

	for (_uint i = 0; i < static_cast<_uint>(m_SceneLights.size()); ++i)
	{
		const SCENE_LIGHT& Light = m_SceneLights[i];

		if (LIGHT::POINT != Light.eType)
			continue;

		_float3 vPosition = _float3(Light.vPosition.x, Light.vPosition.y, Light.vPosition.z);

		ImVec2 vScreenPosition{};
		if (false == World_To_Viewport(vPosition, vImagePos, iViewportWidth, iViewportHeight, &vScreenPosition))
			continue;

		const _bool bSelected = static_cast<_int>(i) == m_iSelectedLightIndex;
		const ImU32 Color = Get_LightColor(Light.eType);

		pDrawList->AddCircleFilled(vScreenPosition, 7.f, Color, 16);
		pDrawList->AddCircle(
			vScreenPosition,
			bSelected ? 16.f : 12.f,
			bSelected ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255),
			16,
			bSelected ? 3.f : 2.f);

		_char szLabel[64] = {};
		sprintf_s(
			szLabel,
			"PointLight %u / Y %.1f / R %.1f",
			i,
			Light.vPosition.y,
			Light.fRange);

		pDrawList->AddText(
			ImVec2(vScreenPosition.x + 10.f, vScreenPosition.y - 8.f),
			Color,
			szLabel);
	}
}

void CNavMeshEditorTool::Select_Light(_float fPickX, _float fPickY, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight)
{
	m_iSelectedLightIndex = INVALID_INDEX;

	if (0 == iViewportWidth || 0 == iViewportHeight)
		return;

	static constexpr _float fPickRadiusPx = 14.f;
	const _float fPickRadiusSq = fPickRadiusPx * fPickRadiusPx;

	const ImVec2 vMouseScreen = ImVec2(vImagePos.x + fPickX, vImagePos.y + fPickY);

	_int iSelected = INVALID_INDEX;
	_float fNearestDistSq = FLT_MAX;

	for (_uint i = 0; i < static_cast<_uint>(m_SceneLights.size()); ++i)
	{
		const SCENE_LIGHT& Light = m_SceneLights[i];

		if (LIGHT::POINT != Light.eType)
			continue;

		const _float3 vLightPosition = _float3(
			Light.vPosition.x,
			Light.vPosition.y,
			Light.vPosition.z);

		ImVec2 vLightScreen{};
		if (false == World_To_Viewport(vLightPosition, vImagePos, iViewportWidth, iViewportHeight, &vLightScreen))
			continue;

		const _float fDX = vLightScreen.x - vMouseScreen.x;
		const _float fDY = vLightScreen.y - vMouseScreen.y;
		const _float fDistSq = fDX * fDX + fDY * fDY;

		if (fDistSq <= fPickRadiusSq && fDistSq < fNearestDistSq)
		{
			fNearestDistSq = fDistSq;
			iSelected = static_cast<_int>(i);
		}
	}

	m_iSelectedLightIndex = iSelected;

	if (INVALID_INDEX != m_iSelectedLightIndex)
		Log_EditStatus(LOG_LEVEL::INFO, "Selected Light.");
}

HRESULT CNavMeshEditorTool::Add_PointLight(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight)
{
	PICK_RESULT Result{};

	if (false == Pick_Surface(fPickX, fPickY, iViewportWidth, iViewportHeight, &Result, true))
	{
		Log_EditStatus(LOG_LEVEL::WARNING, "No map hit.");
		return E_FAIL;
	}

	_float3 vLightPosition = Result.vPosition;
	_bool bSnappedToNavVertex = false;

	if (ImGui::IsKeyDown(ImGuiMod_Ctrl))
	{
		CNavMesh* pNavMesh = Find_NavMesh();

		if (nullptr != pNavMesh)
		{
			const _int iSnapVertexIndex = pNavMesh->Find_Vertex(Result.vPosition);
			const vector<_float3>& Vertices = pNavMesh->Get_Vertices();

			if (iSnapVertexIndex >= 0 &&
				static_cast<size_t>(iSnapVertexIndex) < Vertices.size())
			{
				vLightPosition = Vertices[iSnapVertexIndex];
				bSnappedToNavVertex = true;
			}
		}
	}

	_uint iPointLightCount = 0;

	for (const SCENE_LIGHT& SceneLight : m_SceneLights)
	{
		if (LIGHT::POINT != SceneLight.eType)
			continue;

		if (0 == wcsncmp(SceneLight.szName, TEXT("PointLight"), 10))
			++iPointLightCount;
	}

	_tchar szLightName[MAX_PATH] = {};
	swprintf_s(szLightName, TEXT("PointLight_%02u"), iPointLightCount);

	SCENE_LIGHT Light{};
	Light.eType = LIGHT::POINT;
	wcscpy_s(Light.szName, szLightName);
	Light.vPosition = _float4(vLightPosition.x, vLightPosition.y, vLightPosition.z, 1.f);
	Light.fRange = 16.f;
	Light.vDiffuse = _float4(0.55f, 0.45f, 0.32f, 1.f);
	Light.vAmbient = _float4(0.04f, 0.035f, 0.03f, 1.f);
	Light.vSpecular = _float4(0.20f, 0.18f, 0.14f, 1.f);

	m_SceneLights.push_back(Light);
	m_iSelectedLightIndex = static_cast<_int>(m_SceneLights.size() - 1);

	if (bSnappedToNavVertex)
		Log_EditStatus(LOG_LEVEL::INFO, "Added Point Light. (NavMesh vertex snapped)");
	else
		Log_EditStatus(LOG_LEVEL::INFO, "Added Point Light.");

	return S_OK;
}

CNavMeshEditorTool* CNavMeshEditorTool::Create()
{
	return new CNavMeshEditorTool();
}

void CNavMeshEditorTool::Free()
{
	__super::Free();

	Safe_Release(m_pGameInstance);
}
