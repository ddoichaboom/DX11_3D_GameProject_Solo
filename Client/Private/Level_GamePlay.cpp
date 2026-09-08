#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "Camera_Cinematic.h"
#include "Camera_Follow.h"
#include "Player.h"
#include "Layer.h"
#include "NavMeshObject.h"
#include "NavMesh.h"
#include "Cell.h"
#include "SceneSerializer.h"
#include "UISceneLoader.h"
#include "UI_Image.h"
#include "UI_Video.h"
#include "FadeOverlay_Helper.h"
#include "Monster.h"
#include "Boss_Monster.h"
#include "HUD_GamePlay.h" 
#include "AtlasInstanceEffect.h"
#include "DamageFont.h"

static constexpr _int PLAYER_START_CELL_INDEX = { 40 };

static const _tchar* SCENEDATA_PATH = TEXT("../../Resources/Scenes/Map/ThroneRoom.scene");
static const _tchar* DEFAULT_NAVDATA_PATH = TEXT("../../Resources/NavMesh/ThroneRoom.navdata");
static const _tchar* HUD_SCENE_PATH = TEXT("../../Resources/Scenes/UI/HUD.uiscene");
static const _tchar* THRONEROOM_CUTSCENE_VIDEO_PATH = TEXT("../../Resources/Video/ThroneRoom_CutScene.mp4");
static const _tchar* THRONEROOM_CUTSCENE_BGM_KEY = TEXT("Bgm_Igris_CutScene.wav");
static const _tchar* THRONEROOM_CUTSCENE_LAYER = TEXT("Layer_CutSceneUI");
static const _tchar* THRONEROOM_CUTSCENE_OBJECT = TEXT("CutScene_ThroneRoom_Video");

// 임시로 작성
static void Build_DefaultThroneRoomLights(vector<SCENE_LIGHT>& SceneLights, const SCENE_DATA& SceneData, _bool bSceneDataLoaded)
{
	SceneLights.clear();

	SCENE_LIGHT DirectionalLight{};
	DirectionalLight.eType = LIGHT::DIRECTIONAL;
	wcscpy_s(DirectionalLight.szName, TEXT("Directional_Main"));
	DirectionalLight.vDiffuse = _float4(1.f, 1.f, 1.f, 1.f);
	DirectionalLight.vAmbient = _float4(0.20f, 0.22f, 0.25f, 1.f);
	DirectionalLight.vSpecular = _float4(0.25f, 0.25f, 0.25f, 1.f);
	DirectionalLight.vDirection = _float4(-0.5648625f, -0.8191520f, -0.0996005f, 0.f);
	SceneLights.push_back(DirectionalLight);

	_float3 vPointLightPosition = _float3(0.f, 3.f, 0.f);

	if (true == bSceneDataLoaded)
	{
		if (const SPAWN_POINT* pPlayerSpawnPoint = CSceneSerializer::Find_FirstSpawnPoint(SceneData, SPAWN_TYPE::PLAYER))
		{
			vPointLightPosition = pPlayerSpawnPoint->vPosition;
			vPointLightPosition.y += 3.f;
			vPointLightPosition.z += 2.f;
		}
	}

	SCENE_LIGHT PointLight{};
	PointLight.eType = LIGHT::POINT;
	wcscpy_s(PointLight.szName, TEXT("Point_Default_Player"));
	PointLight.vPosition = _float4(vPointLightPosition.x, vPointLightPosition.y, vPointLightPosition.z, 1.f);
	PointLight.fRange = 25.f;
	PointLight.vDiffuse = _float4(0.85f, 0.75f, 0.55f, 1.f);
	PointLight.vAmbient = _float4(0.08f, 0.07f, 0.05f, 1.f);
	PointLight.vSpecular = _float4(0.25f, 0.22f, 0.18f, 1.f);
	SceneLights.push_back(PointLight);
}

static _bool Is_TorchLightName(const _tchar* pName)
{
	if (nullptr == pName || 0 == pName[0])
		return false;

	return 0 == wcsncmp(pName, TEXT("Torch"), 5);
}

static void Collect_TorchLights(const SCENE_DATA& SceneData, vector<SCENE_LIGHT>& TorchLights)
{
	TorchLights.clear();

	for (const SCENE_LIGHT& SceneLight : SceneData.SceneLights)
	{
		if (LIGHT::POINT != SceneLight.eType)
			continue;

		if (false == Is_TorchLightName(SceneLight.szName))
			continue;

		TorchLights.push_back(SceneLight);
	}
}

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
		if (INVALID_INDEX == Desc.iStartCellIndex)
			Desc.iStartCellIndex = pNavMesh->Find_Cell(Desc.vPosition);

		if (INVALID_INDEX == Desc.iStartCellIndex)
			return false;

		Desc.vPosition.y = pNavMesh->Compute_Height(Desc.iStartCellIndex, Desc.vPosition);
	}

	return true;
}

CNavMesh* CLevel_GamePlay::Find_GamePlayNavMesh() const
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
		if (INVALID_INDEX == Desc.iStartCellIndex)
			Desc.iStartCellIndex = pNavMesh->Find_Cell(Desc.vPosition);

		if (INVALID_INDEX == Desc.iStartCellIndex)
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

HRESULT CLevel_GamePlay::Ready_PlayerTrigger()
{
	m_bPlayerCutsceneTriggerExecuted = false;
	m_bPlayerCutscenePlaying = false;
	m_bPlayerCutsceneFadeOutStarted = false;
	m_fPlayerCutsceneElapsed = 0.f;

	return S_OK;
}

_int CLevel_GamePlay::Get_PlayerNavCellIndex() const
{
	CPlayer* pPlayer = Find_FirstPlayer();
	if (nullptr == pPlayer || nullptr == pPlayer->Get_Transform())
		return INVALID_INDEX;

	CNavMesh* pNavMesh = Find_GamePlayNavMesh();
	if (nullptr == pNavMesh)
		return INVALID_INDEX;

	_float3 vPlayerPosition{};
	XMStoreFloat3(&vPlayerPosition, pPlayer->Get_Transform()->Get_State(STATE::POSITION));

	return pNavMesh->Find_Cell(vPlayerPosition);
}

void CLevel_GamePlay::Update_PlayerTrigger(_float fTimeDelta)
{
	if (true == m_bPlayerCutscenePlaying)
	{
		m_fPlayerCutsceneElapsed += fTimeDelta;

		if (false == m_bPlayerCutsceneFadeOutStarted &&
			PLAYER_CUTSCENE_PLAY_TIME <= m_fPlayerCutsceneElapsed)
		{
			Start_PlayerCutsceneFadeOut();
		}
		return;
	}

	if (true == m_bPlayerCutsceneTriggerExecuted)
		return;

	const _int iPlayerCellIndex = Get_PlayerNavCellIndex();
	if (PLAYER_CUTSCENE_TRIGGER_CELL_INDEX != iPlayerCellIndex)
		return;

	m_bPlayerCutsceneTriggerExecuted = true;
	Execute_PlayerTrigger(iPlayerCellIndex);
}

void CLevel_GamePlay::Execute_PlayerTrigger(_int iTriggerCellIndex)
{
	if (PLAYER_CUTSCENE_TRIGGER_CELL_INDEX != iTriggerCellIndex)
		return;

	Begin_PlayerCutsceneTrigger();
}

void CLevel_GamePlay::Begin_PlayerCutsceneTrigger()
{
	m_bPlayerCutscenePlaying = false;
	m_bPlayerCutsceneFadeOutStarted = false;
	m_fPlayerCutsceneElapsed = 0.f;

	if (CUI_Image* pFade = CFadeOverlay_Helper::Find())
		pFade->Set_Alpha(0.f);

	Start_PlayerCutscenePlayback();
}
void CLevel_GamePlay::Start_PlayerCutscenePlayback()
{
	CUI_Video* pVideo = dynamic_cast<CUI_Video*>(Find_PlayerCutsceneVideo());
	if (nullptr == pVideo)
	{
		if (FAILED(Add_PlayerCutsceneVideo()))
		{
			Start_PlayerCutsceneFadeOut();
			return;
		}
		pVideo = dynamic_cast<CUI_Video*>(Find_PlayerCutsceneVideo());
	}

	if (nullptr == pVideo)
	{
		Start_PlayerCutsceneFadeOut();
		return;
	}

	pVideo->Reset();
	pVideo->Play();

	m_pGameInstance->Play_BGM(THRONEROOM_CUTSCENE_BGM_KEY, 0.7f, false);

	m_fPlayerCutsceneElapsed = 0.f;
	m_bPlayerCutscenePlaying = true;
	m_bPlayerCutsceneFadeOutStarted = false;
}

void CLevel_GamePlay::Start_PlayerCutsceneFadeOut()
{
	m_bPlayerCutsceneFadeOutStarted = true;
	Move_PlayerToNavCell(PLAYER_CUTSCENE_DEST_CELL_INDEX);

	if (CUI_Image* pFade = CFadeOverlay_Helper::Find())
	{
		pFade->Start_Fade(1.f, PLAYER_CUTSCENE_FADE_TIME, [this]() {
			Finish_PlayerCutscene();
			if (CUI_Image* pFadeIn = CFadeOverlay_Helper::Find())
				pFadeIn->Start_Fade(0.f, PLAYER_CUTSCENE_FADE_TIME);
			});
		return;
	}

	Finish_PlayerCutscene();
}

void CLevel_GamePlay::Finish_PlayerCutscene()
{
	m_bPlayerCutscenePlaying = false;
	m_bPlayerCutsceneFadeOutStarted = false;
	m_pGameInstance->Stop_Sound(SOUND_CHANNEL::BGM);

	if (CUI_Video* pVideo = dynamic_cast<CUI_Video*>(Find_PlayerCutsceneVideo()))
		pVideo->Stop();

	CBoss_Monster* pBossMonster = dynamic_cast<CBoss_Monster*>(Find_FirstBossMonster());
	if (nullptr != pBossMonster)
	{
		pBossMonster->Begin_Encounter();
		Play_BossIntroCinematic(pBossMonster);
	}

	m_pGameInstance->Play_BGM(TEXT("BGM_Battle_ThroneRoom_01.wav"), 0.4f, true);
}

HRESULT CLevel_GamePlay::Add_PlayerCutsceneVideo()
{
	CUI_Video::UI_VIDEO_DESC Desc{};
	Desc.fCenterX = 640.f;
	Desc.fCenterY = 360.f;
	Desc.fSizeX = 1280.f;
	Desc.fSizeY = 720.f;
	Desc.iZOrder = 9000;
	Desc.pObjectName = THRONEROOM_CUTSCENE_OBJECT;
	Desc.pVideoPath = THRONEROOM_CUTSCENE_VIDEO_PATH;
	Desc.bLoop = false;
	Desc.fPlaybackSpeed = 1.f;
	Desc.bVisible = false;

	return m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::STATIC), TEXT("Prototype_GameObject_UI_Video"),
		ETOUI(LEVEL::GAMEPLAY), THRONEROOM_CUTSCENE_LAYER, &Desc);
}

CGameObject* CLevel_GamePlay::Find_PlayerCutsceneVideo() const
{
	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(THRONEROOM_CUTSCENE_LAYER);
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& CutSceneObjects = iterLayer->second->Get_GameObjects();
	for (CGameObject* pObject : CutSceneObjects)
	{
		CUIObject* pUI = dynamic_cast<CUIObject*>(pObject);
		if (nullptr != pUI && THRONEROOM_CUTSCENE_OBJECT == pUI->Get_ObjectName())
			return pObject;
	}

	return nullptr;
}

void CLevel_GamePlay::Remove_PlayerCutsceneVideo()
{
	CGameObject* pVideo = Find_PlayerCutsceneVideo();
	if (nullptr != pVideo)
		m_pGameInstance->Remove_GameObject(ETOUI(LEVEL::GAMEPLAY), THRONEROOM_CUTSCENE_LAYER, pVideo);
}

_bool CLevel_GamePlay::Move_PlayerToNavCell(_int iCellIndex)
{
	CPlayer* pPlayer = Find_FirstPlayer();
	CNavMesh* pNavMesh = Find_GamePlayNavMesh();
	if (nullptr == pPlayer || nullptr == pNavMesh)
		return false;

	return pPlayer->Teleport_ToNavCell(pNavMesh, iCellIndex);
}

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

CCamera_Follow* CLevel_GamePlay::Find_FollowCamera() const
{
	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_Camera"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	for (CGameObject* pObject : iterLayer->second->Get_GameObjects())
	{
		CCamera_Follow* pCamera = dynamic_cast<CCamera_Follow*>(pObject);
		if (nullptr != pCamera)
			return pCamera;
	}

	return nullptr;
}

CCamera_Cinematic* CLevel_GamePlay::Find_CinematicCamera() const
{
	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_Camera"));
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	for (CGameObject* pObject : iterLayer->second->Get_GameObjects())
	{
		CCamera_Cinematic* pCamera = dynamic_cast<CCamera_Cinematic*>(pObject);
		if (nullptr != pCamera)
			return pCamera;
	}

	return nullptr;
}

void CLevel_GamePlay::Play_BossIntroCinematic(CBoss_Monster* pBossMonster) const
{
	if (nullptr == pBossMonster)
		return;

	CCamera_Cinematic* pCinematic = Find_CinematicCamera();
	if (nullptr == pCinematic)
		return;

	pCinematic->Set_ReturnCamera(Find_FollowCamera());

	static const _char* PivotCandidates[] =
	{
			"Cam_Pivot_Chest",
			"CamPivot_Chest",
			"Pivot_Chest",
	};

	for (const _char* pPivotName : PivotCandidates)
	{
		CCamera_Cinematic::ATTACH_DESC Desc{};
		Desc.eMode = CCamera_Cinematic::MODE::ATTACH_PIVOT;
		Desc.StartAnchor.pObject = pBossMonster;
		Desc.StartAnchor.pPartTag = TEXT("Body");
		Desc.StartAnchor.pPivotName = pPivotName;
		Desc.vLocalOffset = _float3(0.f, -0.45f, 6.8f);
		Desc.vLookOffset = _float3(0.f, 0.35f, 0.f);
		Desc.fDuration = 2.2f;
		Desc.fFovy = XMConvertToRadians(40.f);
		Desc.bUsePivotRotation = false;

		if (SUCCEEDED(pCinematic->Play(Desc)))
			return;
	}
}

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

	if (FAILED(Ready_Layer_Effect(TEXT("Layer_Effect"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
		return E_FAIL;

	if (FAILED(Ready_CollisionGroup()))
		return E_FAIL;

	if (FAILED(Ready_PlayerTrigger()))
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
	Update_PlayerTrigger(fTimeDelta);

#ifdef _DEBUG
	CMonster* pBossMonster = Find_FirstBossMonster();

	if (nullptr != pBossMonster)
	{
		if (m_pGameInstance->Get_KeyDown('5'))
			//pBossMonster->Debug_TryAction(MONSTER_ACTION::CRASH, MONSTER_ACTION_STEP::START);
			pBossMonster->Force_Break();

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

			if (m_pGameInstance->Get_KeyDown(VK_F1))
				m_pGameInstance->Toggle_RenderCollider();
			if (m_pGameInstance->Get_KeyDown(VK_F2))
				m_pGameInstance->Toggle_RenderNavMesh();
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
	vector<SCENE_LIGHT> DefaultLights;
	const vector<SCENE_LIGHT>* pSceneLights = &m_SceneData.SceneLights;

	if (false == m_bSceneDataLoaded || true == m_SceneData.SceneLights.empty())
	{
		Build_DefaultThroneRoomLights(DefaultLights, m_SceneData, m_bSceneDataLoaded);
		pSceneLights = &DefaultLights;
	}

	for (const SCENE_LIGHT& SceneLight : *pSceneLights)
	{
		LIGHT_DESC LightDesc{};

		LightDesc.eType = SceneLight.eType;
		LightDesc.vDiffuse = SceneLight.vDiffuse;
		LightDesc.vAmbient = SceneLight.vAmbient;
		LightDesc.vSpecular = SceneLight.vSpecular;
		LightDesc.vDirection = SceneLight.vDirection;
		LightDesc.vPosition = SceneLight.vPosition;
		LightDesc.fRange = SceneLight.fRange;

		if (FAILED(m_pGameInstance->Add_Light(LightDesc)))
			return E_FAIL;
	}

	SHADOW_LIGHT_DESC ShadowDesc{};

	ShadowDesc.vEye = _float4(-220.f, 120.f, -380.f, 1.f);
	ShadowDesc.vAt = _float4(-130.f, 1.f, -272.f, 1.f);
	ShadowDesc.fFovy = XMConvertToRadians(40.f);
	ShadowDesc.fNear = 1.f;
	ShadowDesc.fFar = 300.f;
	ShadowDesc.fAspect = static_cast<_float>(m_pGameInstance->Get_WinSizeX()) /
		static_cast<_float>(m_pGameInstance->Get_WinSizeY());
	ShadowDesc.bOrthographic = false;
	ShadowDesc.fOrthoWidth = 100.f;
	ShadowDesc.fOrthoHeight = 100.f;

	if (FAILED(m_pGameInstance->Add_ShadowLight(ShadowDesc)))
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
	CameraDesc.pCamColliderVertices = &m_SceneData.CamColliderVertices;
	CameraDesc.pCamColliderFaces = &m_SceneData.CamColliderFaces;

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_Camera_Follow"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag, &CameraDesc)))
		return E_FAIL;

	CCamera_Cinematic::CAMERA_CINEMATIC_DESC CinematicDesc{};
	CinematicDesc.vEye = _float3(0.f, 3.f, -5.f);
	CinematicDesc.vAt = _float3(0.f, 1.5f, 0.f);
	CinematicDesc.fFovy = XMConvertToRadians(45.f);
	CinematicDesc.fNear = 0.1f;
	CinematicDesc.fFar = 500.f;
	CinematicDesc.bActive = false;

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_Camera_Cinematic"),
		ETOUI(LEVEL::GAMEPLAY), strLayerTag, &CinematicDesc)))
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
	Desc.iStartCellIndex = INVALID_INDEX;

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

HRESULT CLevel_GamePlay::Ready_Layer_Effect(const _wstring& strLayerTag)
{
	if (false == m_bSceneDataLoaded)
		return S_OK;

	vector<SCENE_LIGHT> TorchLights;
	Collect_TorchLights(m_SceneData, TorchLights);

	if (true == TorchLights.empty())
		return S_OK;

	vector<_float4> TorchPositions;
	TorchPositions.reserve(TorchLights.size());

	for (const SCENE_LIGHT& TorchLight : TorchLights)
		TorchPositions.push_back(TorchLight.vPosition);

	CAtlasInstanceEffect::ATLAS_INSTANCE_EFFECT_DESC Desc{};
	Desc.pTexturePrototypeTag = TEXT("Prototype_Component_Texture_Effect_Fire_Atlas");
	Desc.pPositions = &TorchPositions;
	Desc.iMaxInstanceCount = static_cast<_uint>(TorchPositions.size());
	Desc.iAtlasCols = 6;
	Desc.iAtlasRows = 6;
	Desc.fFrameDuration = 0.045f;
	Desc.vSize = _float2(1.0f, 1.2f);
	Desc.vUVPadding = _float2(0.002f, 0.002f);
	Desc.vColor = _float4(1.35f, 0.85f, 0.45f, 1.f);
	Desc.fAlpha = 0.95f;
	Desc.iShaderPass = 1;
	Desc.bLoop = true;

	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_AtlasInstanceEffect"),
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

	CDamageFont::DAMAGEFONT_DESC DamageDesc{};
	DamageDesc.pTextureProtoTag = TEXT("Prototype_Component_Texture_HUD_Combo_Digit");
	DamageDesc.iAtlasCols = 10;
	DamageDesc.iAtlasRows = 1;
	DamageDesc.iMaxInstanceCount = 128;
	if (FAILED(m_pGameInstance->Add_GameObject(
		ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_DamageFont"),
		ETOUI(LEVEL::GAMEPLAY), TEXT("Layer_Effect"), &DamageDesc)))
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
