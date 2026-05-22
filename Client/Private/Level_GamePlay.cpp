#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "Camera_Follow.h"
#include "Player.h"
#include "Layer.h"
#include "NavMeshObject.h"
#include "NavMesh.h"
#include "Cell.h"
#include "SceneSerializer.h"
#include "UISceneLoader.h"
#include "UI_Image.h"
#include "FadeOverlay_Helper.h"
#include "Monster.h"
#include "HUD_GamePlay.h" 

static constexpr _int PLAYER_START_CELL_INDEX = { 40 };

static const _tchar* SCENEDATA_PATH = TEXT("../../Resources/Scenes/Map/ThroneRoom.scene");
static const _tchar* DEFAULT_NAVDATA_PATH = TEXT("../../Resources/NavMesh/ThroneRoom.navdata");
static const _tchar* HUD_SCENE_PATH = TEXT("../../Resources/Scenes/UI/HUD.uiscene");

_bool CLevel_GamePlay::Apply_PlayerSpawnFromCell(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, _int iCellIndex)
{
	if (nullptr == pNavMesh)
		return false;

	const CCell* pStartCell = pNavMesh->Get_Cell(iCellIndex);
	if (nullptr == pStartCell)
		return false;

	Desc.vPosition = pStartCell->Get_Center();
	Desc.vPosition.y = pNavMesh->Compute_Height(iCellIndex, Desc.vPosition);
	Desc.iStartCellIndex = iCellIndex;

	return true;
}

_bool CLevel_GamePlay::Apply_PlayerSpawnPoint(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT* pSpawnPoint)
{
	if (nullptr == pSpawnPoint)
		return false;

	Desc.vPosition = pSpawnPoint->vPosition;
	Desc.vRotationDeg = pSpawnPoint->vRotationDeg;
	Desc.iStartCellIndex = pSpawnPoint->iNavCellIndex;

	if (nullptr != pNavMesh)
	{
		if (NAVMESH_INVALID_INDEX == Desc.iStartCellIndex)
			Desc.iStartCellIndex = pNavMesh->Find_Cell(Desc.vPosition);

		if (NAVMESH_INVALID_INDEX == Desc.iStartCellIndex)
			return false;

		Desc.vPosition.y = pNavMesh->Compute_Height(Desc.iStartCellIndex, Desc.vPosition);
	}

	return true;
}

CNavMesh* CLevel_GamePlay::Find_GamePlayNavMesh()
{
	if (nullptr == m_pGameInstance)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_NavMesh"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& NavMeshObjects = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : NavMeshObjects)
	{
		CNavMeshObject* pNavMeshObject = dynamic_cast<CNavMeshObject*>(pObject);
		if (nullptr == pNavMeshObject)
			continue;

		CNavMesh* pNavMesh = pNavMeshObject->Get_NavMesh();
		if (nullptr != pNavMesh)
			return pNavMesh;
	}

	return nullptr;
}

const _tchar* CLevel_GamePlay::Get_MonsterPrototypeTag(SPAWN_TYPE eType) const
{
	switch (eType)
	{
	case SPAWN_TYPE::MONSTER_NORMAL:
		return TEXT("Prototype_GameObject_Normal_Monster");

	case SPAWN_TYPE::MONSTER_ELITE:
		return TEXT("Prototype_GameObject_Elite_Monster");

	case SPAWN_TYPE::MONSTER_BOSS:
		return TEXT("Prototype_GameObject_Boss_Monster");

	default:
		return nullptr;
	}
}

_bool CLevel_GamePlay::Apply_MonsterSpawnPoint(CMonster::MONSTER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT& SpawnPoint)
{
	Desc.vPosition = SpawnPoint.vPosition;
	Desc.vRotationDeg = SpawnPoint.vRotationDeg;
	Desc.iStartCellIndex = SpawnPoint.iNavCellIndex;
	Desc.eSpawnType = SpawnPoint.eType;

	if (nullptr != pNavMesh)
	{
		if (NAVMESH_INVALID_INDEX == Desc.iStartCellIndex)
			Desc.iStartCellIndex = pNavMesh->Find_Cell(Desc.vPosition);

		if (NAVMESH_INVALID_INDEX == Desc.iStartCellIndex)
			return false;

		Desc.vPosition.y = pNavMesh->Compute_Height(Desc.iStartCellIndex, Desc.vPosition);
	}

	Desc.iLevel = SpawnPoint.iLevel;
	wcscpy_s(Desc.szDisplayName, SpawnPoint.szDisplayName);

	return true;
}

CPlayer* CLevel_GamePlay::Find_FirstPlayer() const
{
	if (nullptr == m_pGameInstance)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_Player"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& PlayerObjects = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : PlayerObjects)
	{
		CPlayer* pPlayer = dynamic_cast<CPlayer*>(pObject);
		if (nullptr != pPlayer)
			return pPlayer;
	}

	return nullptr;
}

#ifdef _DEBUG
CMonster* CLevel_GamePlay::Find_FirstBossMonster() const
{
	if (nullptr == m_pGameInstance)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_Monster"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& MonsterObjects = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : MonsterObjects)
	{
		CMonster* pMonster = dynamic_cast<CMonster*>(pObject);
		if (nullptr == pMonster)
			continue;

		if (SPAWN_TYPE::MONSTER_BOSS == pMonster->Get_SpawnType())
			return pMonster;
	}

	return nullptr;
}
#endif

CLevel_GamePlay::CLevel_GamePlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	:CLevel{ pDevice, pContext }
{

}

HRESULT CLevel_GamePlay::Initialize()
{
	if (FAILED(Ready_SceneData()))
		return E_FAIL;

	if (FAILED(Ready_Lights()))
		return E_FAIL;

	if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_NavMesh(TEXT("Layer_NavMesh"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
		return E_FAIL;

	if (FAILED(Ready_CollisionGroup()))
		return E_FAIL;

	if (CUI_Image* pFade = CFadeOverlay_Helper::Find())
	{
		pFade->Set_Alpha(1.f);
		pFade->Start_Fade(0.f, 0.5f);
	}

	m_pGameInstance->Set_CursorLocked(true);

	return S_OK;
}

void CLevel_GamePlay::Update(_float fTimeDelta)
{
#ifdef _DEBUG
	CMonster* pBossMonster = Find_FirstBossMonster();

	if (nullptr != pBossMonster)
	{
		if (m_pGameInstance->Get_KeyDown('6'))
			pBossMonster->Debug_TryAction(MONSTER_ACTION::SKILL_10, MONSTER_ACTION_STEP::START);

		if (m_pGameInstance->Get_KeyDown('7'))
			pBossMonster->Debug_TryAction(MONSTER_ACTION::BASIC_ATTACK_01);

		if (m_pGameInstance->Get_KeyDown('8'))
			pBossMonster->Debug_TryAction(MONSTER_ACTION::SKILL_09, MONSTER_ACTION_STEP::START);

		if (m_pGameInstance->Get_KeyDown('9'))
			pBossMonster->Debug_TryAction(MONSTER_ACTION::DEATH);

		if (m_pGameInstance->Get_KeyDown('0'))
			pBossMonster->Debug_TryAction(MONSTER_ACTION::SKILL_09, MONSTER_ACTION_STEP::END);
	}

	CPlayer* pPlayer = Find_FirstPlayer();

	if (nullptr != pPlayer)
	{
		if (m_pGameInstance->Get_KeyDown(VK_F6))
			pPlayer->Enter_FloatReaction(CHARACTER_ACTION::FLOAT_A);

		if (m_pGameInstance->Get_KeyDown(VK_F7))
			pPlayer->Enter_FloatReaction(CHARACTER_ACTION::FLOAT_B);
	}
#endif
}

HRESULT CLevel_GamePlay::Render()
{
	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_SceneData()
{
	m_SceneData = SCENE_DATA{};
	m_bSceneDataLoaded = false;

	if (SUCCEEDED(CSceneSerializer::Load(SCENEDATA_PATH, &m_SceneData)))
		m_bSceneDataLoaded = true;

	if (0 == m_SceneData.szNavDataPath[0])
		wcscpy_s(m_SceneData.szNavDataPath, DEFAULT_NAVDATA_PATH);

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Lights()
{
	LIGHT_DESC LightDesc{};

	LightDesc.eType = LIGHT::DIRECTIONAL;
	LightDesc.vDiffuse = _float4(1.f, 1.f, 1.f, 1.f);
	LightDesc.vAmbient = _float4(0.20f, 0.22f, 0.25f, 1.f);
	//LightDesc.vSpecular = _float4(0.7924528f, 0.7924528f, 0.7924528f, 1.f);
	LightDesc.vSpecular = _float4(0.25f, 0.25f, 0.25f, 1.f);

	// ThroneRoom Fog_ThroneR_01 sunDirection = (55, 260, 0) converted to engine light direction.
	LightDesc.vDirection = _float4(-0.5648625f, -0.8191520f, -0.0996005f, 0.f);

	if (FAILED(m_pGameInstance->Add_Light(LightDesc)))
		return E_FAIL;


	// 임시 
	LIGHT_DESC PointLightDesc{};

	PointLightDesc.eType = LIGHT::POINT;
	_float3 vPointLightPosition = _float3(0.f, 3.f, 0.f);

	if (m_bSceneDataLoaded)
	{
		if (const SPAWN_POINT* pPlayerSpawnPoint = CSceneSerializer::Find_FirstSpawnPoint(m_SceneData, SPAWN_TYPE::PLAYER))
		{
			vPointLightPosition = pPlayerSpawnPoint->vPosition;
			vPointLightPosition.y += 3.f;
			vPointLightPosition.z += 2.f;
		}
	}

	PointLightDesc.vPosition = _float4(
		vPointLightPosition.x,
		vPointLightPosition.y,
		vPointLightPosition.z,
		1.f
	);	
	PointLightDesc.fRange = 25.f;
	PointLightDesc.vDiffuse = _float4(0.85f, 0.75f, 0.55f, 1.f);
	PointLightDesc.vAmbient = _float4(0.08f, 0.07f, 0.05f, 1.f);
	//PointLightDesc.vSpecular = _float4(0.8f, 0.7f, 0.55f, 1.f);
	PointLightDesc.vSpecular = _float4(0.25f, 0.22f, 0.18f, 1.f);

	if (FAILED(m_pGameInstance->Add_Light(PointLightDesc)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer_Camera(const _wstring& strLayerTag)
{
	const _float4x4* pTargetWorld = { nullptr };

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));

	if (nullptr != pLayers)
	{
		auto iter = pLayers->find(TEXT("Layer_Player"));
		if (iter != pLayers->end() && nullptr != iter->second)
		{
			const list<CGameObject*>& PlayerObjects = iter->second->Get_GameObjects();
			if (false == PlayerObjects.empty())
			{
				CGameObject* pPlayer = PlayerObjects.front();
				if (nullptr != pPlayer && nullptr != pPlayer->Get_Transform())
					pTargetWorld = pPlayer->Get_Transform()->Get_WorldMatrixPtr();
			}
		}
	}

	CCamera_Follow::CAMERA_FOLLOW_DESC  CameraDesc{};
	CameraDesc.vEye = _float3(0.f, 3.f, -5.f);
	CameraDesc.vAt = _float3(0.f, 1.5f, 0.f);
	CameraDesc.fFovy = XMConvertToRadians(60.f);
	CameraDesc.fNear = 0.1f;
	CameraDesc.fFar = 500.f;
	CameraDesc.strTargetLayerTag = TEXT("Layer_Player");
	CameraDesc.pTargetWorldMatrix = pTargetWorld;
	CameraDesc.vHeightOffset = { 0.f, 1.5f, 0.f };
	CameraDesc.fIdealDistance = 5.f;
	CameraDesc.fInitialYaw = 0.f;
	CameraDesc.fInitialPitch = -0.3f;
	CameraDesc.fPitchMin = -1.1f;
	CameraDesc.fPitchMax = 1.0f;
	CameraDesc.fMouseSensor = 0.003f;
	CameraDesc.fArmLerpSpeed = 8.f;

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_Camera_Follow"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag, &CameraDesc)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer_BackGround(const _wstring& strLayerTag)
{
	// Prototype_GameObject_MapObject
	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_MapObject"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer_NavMesh(const _wstring& strLayerTag)
{
	NAVMESH_SNAPSHOT Snapshot{};

	if (FAILED(CNavMesh::Load_NavDataSnapshot(
		m_SceneData.szNavDataPath,
		&Snapshot)))
		return E_FAIL;

	CNavMeshObject::NAVMESHOBJECT_DESC Desc{};
	Desc.pInitialSnapshot = &Snapshot;

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_NavMeshObject"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag, &Desc)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer_Monster(const _wstring& strLayerTag)
{
	if (false == m_bSceneDataLoaded)
		return S_OK;

	CNavMesh* pNavMesh = Find_GamePlayNavMesh();
	CPlayer* pTargetPlayer = Find_FirstPlayer();

	for (const SPAWN_POINT& SpawnPoint : m_SceneData.SpawnPoints)
	{
		const _tchar* pPrototypeTag = Get_MonsterPrototypeTag(SpawnPoint.eType);
		if (nullptr == pPrototypeTag)
			continue;

		CMonster::MONSTER_DESC Desc{};
		Desc.pNavMesh = pNavMesh;
		Desc.pTarget = pTargetPlayer;

		if (false == Apply_MonsterSpawnPoint(Desc, pNavMesh, SpawnPoint))
			continue;

		if (FAILED(m_pGameInstance->Add_GameObject(
			ETOUI(LEVEL::GAMEPLAY), pPrototypeTag,
			ETOUI(LEVEL::GAMEPLAY), strLayerTag, &Desc)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_Layer_Player(const _wstring& strLayerTag)
{
	CPlayer::PLAYER_DESC Desc{};

	CNavMesh* pNavMesh = Find_GamePlayNavMesh();

	Desc.pNavMesh = pNavMesh;
	Desc.iStartCellIndex = NAVMESH_INVALID_INDEX;

	Desc.vPosition = _float3(0.f, 1.f, 0.f);
	Desc.vRotationDeg = _float3(0.f, 0.f, 0.f);
	Desc.vScale = _float3(1.15f, 1.15f, 1.15f);
	Desc.fSpeedPerSec = 5.f;
	Desc.fRotationPerSec = XMConvertToRadians(1440.f);

	const SPAWN_POINT* pPlayerSpawnPoint = nullptr;

	if (m_bSceneDataLoaded)
		pPlayerSpawnPoint = CSceneSerializer::Find_FirstSpawnPoint(m_SceneData, SPAWN_TYPE::PLAYER);

	if (false == Apply_PlayerSpawnPoint(Desc, pNavMesh, pPlayerSpawnPoint))
		Apply_PlayerSpawnFromCell(Desc, pNavMesh, PLAYER_START_CELL_INDEX);

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_Player"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag, &Desc)))
		return E_FAIL;

	return S_OK;
}

HRESULT	 CLevel_GamePlay::Ready_Layer_UI(const _wstring& strLayerTag)
{
	if (FAILED(CUISceneLoader::Load_Into_Layer(
		HUD_SCENE_PATH,
		ETOUI(LEVEL::GAMEPLAY),
		strLayerTag)))
	{
		// 파일 없어도 일단 진행
		return S_OK;
	}

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::STATIC), TEXT("Prototype_GameObject_HUD_GamePlay"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_GamePlay::Ready_CollisionGroup()
{
	m_pGameInstance->Set_CollisionMatrix(
		COLLISION_GROUP::PLAYER_BODY,
		COLLISION_GROUP::MONSTER_BODY,
		true);

	m_pGameInstance->Set_CollisionMatrix(
		COLLISION_GROUP::PLAYER_ATTACK,
		COLLISION_GROUP::MONSTER_BODY,
		true);

	m_pGameInstance->Set_CollisionMatrix(
		COLLISION_GROUP::MONSTER_ATTACK,
		COLLISION_GROUP::PLAYER_BODY,
		true);

	return S_OK;
}

CLevel_GamePlay* CLevel_GamePlay::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CLevel_GamePlay* pInstance = new CLevel_GamePlay(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CLevel_GamePlay");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CLevel_GamePlay::Free()
{
	m_pGameInstance->Set_CursorLocked(false);

	__super::Free();
}