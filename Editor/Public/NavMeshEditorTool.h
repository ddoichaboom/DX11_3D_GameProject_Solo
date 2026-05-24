#pragma once

#include "Editor_Defines.h"
#include "Base.h"
#include "NavMesh_Types.h"
#include "Client_Defines.h"

NS_BEGIN(Engine)
class CGameInstance;
class CGameObject;
class CNavMesh;
NS_END

NS_BEGIN(Editor)

class CNavMeshEditorTool final : public CBase
{
private:
	typedef struct tagPickResult
	{
		CGameObject* pObject = { nullptr };
		_float3		vPosition = {};
		_float		fDistance = {};
	}PICK_RESULT;

	typedef struct tagNavMeshPickPoint
	{
		_float3		vRawPosition = {};
		_float3		vPreviewPosition = {};
		_int		iSnapVertexIndex = { INVALID_INDEX };
		_bool		bSnapped = { false };
	}NAVMESH_PICK_POINT;

private:
	CNavMeshEditorTool();
	virtual ~CNavMeshEditorTool() = default;

public:
	void					Render_Overlay(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	void					Handle_ViewportClick(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);
	void                    Handle_LightViewportClick(_float fPickX, _float fPickY, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);

	HRESULT					Create_NavMeshCell();
	void					Clear_PickPoints();
	HRESULT					Delete_SelectedCell();
	HRESULT					Undo();
	HRESULT					Redo();
	HRESULT					Save_NavData();
	HRESULT					Load_NavData();

	HRESULT					Set_PlayerSpawnPoint();
	HRESULT					Add_MonsterSpawnPoint(SPAWN_TYPE eType);
	HRESULT					Add_DefaultDirectionalLight();

	HRESULT					Delete_SelectedLight();
	HRESULT					Delete_LastSpawnPoint();
	HRESULT					Delete_SelectedSpawnPoint();

	void					Clear_SpawnPoints();
	void					Clear_Lights();
	HRESULT					Save_SceneData();
	HRESULT					Load_SceneData();

	_int					Get_SelectedCellIndex() const { return m_iSelectedNavMeshCellIndex; }
	_int					Get_SelectedVertexIndex() const { return m_iSelectedNavMeshVertexIndex; }
	_int					Get_SelectedSpawnPointIndex() const { return m_iSelectedSpawnPointIndex; }
	_uint					Get_NumPickPoints() const { return static_cast<_uint>(m_NavMeshPickedPoints.size()); }
	_uint					Get_NumSpawnPoints() const { return static_cast<_uint>(m_SpawnPoints.size()); }
	_bool					Has_SpawnPoint() const { return false == m_SpawnPoints.empty(); }
	const SPAWN_POINT*		Get_SpawnPoint(_uint iIndex) const;

	_uint					Get_NumSceneLights() const { return static_cast<_uint>(m_SceneLights.size()); }
	_int					Get_SelectedLightIndex() const { return m_iSelectedLightIndex; }
	const SCENE_LIGHT*		Get_SceneLight(_uint iIndex) const;

	void                    Set_SelectedLightIndex(_int iIndex);
	void                    Set_SelectedSceneLight(const SCENE_LIGHT& Light);


	void					Set_SelectedSpawnPointIndex(_int iIndex);
	void					Set_SelectedSpawnPointRotation(const _float3& vRotationDeg);
	void					Set_SelectedSpawnPointYaw(_float fYawDeg);

	_bool					Can_Undo() const { return false == m_NavMeshUndoStack.empty(); }
	_bool					Can_Redo() const { return false == m_NavMeshRedoStack.empty(); }

	void					Set_SelectedSpawnPointLevel(_int iLevel);
	void					Set_SelectedSpawnPointDisplayName(const _tchar* pName);

private:
	void					Render_PickPreview(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	void					Render_SelectedCell(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	void					Render_SelectedVertex(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	void					Render_SpawnPoints(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);

	void					Select_Vertex(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);
	HRESULT					Move_SelectedVertex(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);
	void					Select_Cell(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);
	void					Pick_EditPoint(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);

	_bool					Build_SpawnPointFromSelectedCell(SPAWN_TYPE eType, const _tchar* pName, SPAWN_POINT* pOutPoint);
	void					Push_OrReplacePlayerSpawnPoint(const SPAWN_POINT& Point);

	void					Push_UndoSnapshot(const NAVMESH_SNAPSHOT& Snapshot);
	void					Clear_EditState();

	_bool					World_To_Viewport(const _float3& vWorldPosition, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight, ImVec2* pOutScreenPosition) const;
	_bool					Pick_Surface(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight, PICK_RESULT* pOutResult, _bool bMapOnly = false);
	CNavMesh*				Find_NavMesh() const;
	_bool					Get_CurrentNavDataPath(_tchar* pOutPath, size_t iLength) const;

	void					Log_EditStatus(LOG_LEVEL eLevel, const string& strMessage) const;

	void                    Render_Lights(const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	void                    Select_Light(_float fPickX, _float fPickY, const ImVec2& vImagePos, _uint iViewportWidth, _uint iViewportHeight);
	HRESULT                 Add_PointLight(_float fPickX, _float fPickY, _uint iViewportWidth, _uint iViewportHeight);

private:
	CGameInstance*			m_pGameInstance = { nullptr };

	_bool					m_bHasLastNavMeshPick = { false };
	_float3					m_vLastNavMeshPick = {};
	vector<NAVMESH_PICK_POINT> m_NavMeshPickedPoints;

	_int					m_iSelectedNavMeshCellIndex = { INVALID_INDEX };
	_int					m_iSelectedNavMeshVertexIndex = { INVALID_INDEX };
	vector<NAVMESH_SNAPSHOT> m_NavMeshUndoStack;
	vector<NAVMESH_SNAPSHOT> m_NavMeshRedoStack;
	vector<SPAWN_POINT>		m_SpawnPoints;
	_int					m_iSelectedSpawnPointIndex = { INVALID_INDEX };

	vector<SCENE_LIGHT>		m_SceneLights;
	_int					m_iSelectedLightIndex = { INVALID_INDEX };

public:
	static CNavMeshEditorTool* Create();
	virtual void			Free() override;
};

NS_END
