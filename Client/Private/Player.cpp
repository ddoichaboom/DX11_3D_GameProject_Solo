#include "Player.h"
#include "GameInstance.h"
#include "Body_Player.h"
#include "Weapon.h"
#include "Transform_3D.h"
#include "IntentResolver.h"
#include "Player_StateMachine.h"
#include "NavigationAgent.h"
#include "NavMesh.h"
#include "Monster.h"
#include "Collider.h"
#include "Layer.h"
#include "Cell.h"
#include "HUD_GamePlay.h"

namespace
{
	static constexpr _float PLAYER_BODY_BLOCK_RADIUS = { 0.40f };
	static constexpr _float MONSTER_NORMAL_BODY_BLOCK_RADIUS = { 0.45f };
	static constexpr _float MONSTER_ELITE_BODY_BLOCK_RADIUS = { 0.55f };
	static constexpr _float MONSTER_BOSS_BODY_BLOCK_RADIUS = { 0.80f };
	static constexpr _float BODY_BLOCK_SKIN = { 0.05f };
	static constexpr _float BODY_BLOCK_MIN_MOVE_SQ = { 0.000001f };

	static const WEAPON_INFO s_WeaponInfo[] = {
		{EQUIPPED_WEAPON_ID::NONE, WEAPON_TYPE::DEFAULT, nullptr },
		{EQUIPPED_WEAPON_ID::KNIGHT_KILLER, WEAPON_TYPE::DAGGER, TEXT("Prototype_Component_Model_Weapon_KnightKiller") },
		{EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG, WEAPON_TYPE::DAGGER, TEXT("Prototype_Component_Model_Weapon_KasakaVenomFang") },
	};

	// 양손 단검 디폴트 풀 - DEFAULT/DAGGER 장착 시 보조 손에 사용
	static const _tchar* DAGGER_POOL[] = {
		TEXT("Prototype_Component_Model_Weapon_KnightKiller"),
		TEXT("Prototype_Component_Model_Weapon_KasakaVenomFang"),
	};
}

CPlayer::CPlayer(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CContainerObject{ pDevice, pContext }
{
}

CPlayer::CPlayer(const CPlayer& Prototype)
	: CContainerObject{ Prototype }
{
}

void CPlayer::Take_Damage(_float fAmount)
{
	if (m_fCurrentHP <= 0.f)
		return;

	m_fCurrentHP = max(0.f, m_fCurrentHP - fAmount);

	if (auto* pHUD = CHUD_GamePlay::Get_Instance())
		pHUD->Notify_CombatInput();
}

_bool CPlayer::Try_GetDashHUDWorldPosition(_float3* pOutPosition) const
{
	if (nullptr == pOutPosition || nullptr == m_pBody)
		return false;

	const _float4x4* pPivot = m_pBody->Get_BoneMatrixPtr("Pivot_Chest");
	if (nullptr == pPivot)
		return false;

	_matrix PivotWorld =
		XMLoadFloat4x4(pPivot) *
		XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr());

	XMStoreFloat3(pOutPosition, XMVector3TransformCoord(XMVectorZero(), PivotWorld));
	return true;
}

_bool CPlayer::Try_Teleport(_float fSearchRadius, _float fConeAngleDegrees)
{
	CMonster* pTarget = Find_Target(fSearchRadius, fConeAngleDegrees);
	if (nullptr == pTarget)
		return false;   // 호출자 측에서 그대로 액션 진입 (제자리 재생)

	Teleport_BehindTarget(pTarget);
	return true;
}

HRESULT CPlayer::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CPlayer::Initialize(void* pArg)
{
	m_strName = TEXT("Player");
	m_strTag = TEXT("SungJinWoo");

	PLAYER_DESC Desc{};

	if (nullptr != pArg)
		Desc = *static_cast<PLAYER_DESC*>(pArg);

	if (0.f == Desc.fSpeedPerSec)
		Desc.fSpeedPerSec = 10.f;

	if (0.f == Desc.fRotationPerSec)
		Desc.fRotationPerSec = XMConvertToRadians(180.f);

	if (FAILED(__super::Initialize(&Desc)))
		return E_FAIL;

	if (FAILED(Ready_Components(Desc)))
		return E_FAIL;

	if (FAILED(Ready_PartObjects()))
		return E_FAIL;

	m_pIntentResolver = CIntentResolver::Create();
	if (nullptr == m_pIntentResolver)
		return E_FAIL;

	if (FAILED(Ready_StateMachine()))
		return E_FAIL;

	Set_EquippedWeapon(EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG);

	return S_OK;
}

void CPlayer::Priority_Update(_float fTimeDelta)
{
	for (auto& Pair : m_PartObjects)
	{
		if (nullptr != Pair.second)
			Pair.second->Priority_Update(fTimeDelta);
	}
}

void CPlayer::Update(_float fTimeDelta)
{
	if (false == m_pGameInstance->Is_GameLogic_Frozen())
	{
		Tick_DashRegen(fTimeDelta);
		Tick_WeaponHideTimer(fTimeDelta);
		Tick_SkillCooldowns(fTimeDelta);

		PLAYER_RAW_INPUT_FRAME Raw{};
		Gather_RawInput(&Raw);

		if (true == Raw.bRButtonHeld)
			m_fGuardHoldGraceTimer = GUARD_HOLD_GRACE;
		else
			m_fGuardHoldGraceTimer = max(0.f, m_fGuardHoldGraceTimer - fTimeDelta);

		Raw.bRButtonHeld = (m_fGuardHoldGraceTimer > 0.f);

		if (true == Raw.bLButtonPressed)
			m_fAttackBufferTimer = ATTACK_BUFFER_DURATION;
		else
			m_fAttackBufferTimer = max(0.f, m_fAttackBufferTimer - fTimeDelta);

		Raw.bLButtonPressed = (m_fAttackBufferTimer > 0.f);

		PLAYER_INTENT_FRAME Intent{};

		const _float fCameraYaw = Query_CameraYaw();

		if (nullptr != m_pIntentResolver)
			m_pIntentResolver->Resolve(Raw, fCameraYaw, &Intent);

		if (nullptr != m_pStateMachine)
		{
			if (true == m_pStateMachine->Is_ReactionLocked())
			{
				m_pStateMachine->Update_Reaction(Intent);
			}
			else
			{
				m_pStateMachine->Update_Skills(Intent);
				m_pStateMachine->Update_Guard(Intent);
				m_pStateMachine->Update_Combat(Intent);

				const _bool bLocomotionAllowed =
					(false == m_pStateMachine->Is_AttackLocked() &&
						false == m_pStateMachine->Is_GuardLocked()) ||
					true == Intent.bDashRequested;

				if (true == bLocomotionAllowed)
				{
					m_pStateMachine->Update_LocoMotion(Intent);
				}
			}

			m_pStateMachine->Update(fTimeDelta);
		}

		for (auto& Pair : m_PartObjects)
		{
			if (nullptr != Pair.second)
				Pair.second->Update(fTimeDelta);
		}

		if (nullptr != m_pBody)
			Apply_RootMotion(m_pBody->Get_LastRootMotionDelta());

		Apply_MoveIntent(Intent, fTimeDelta);
	}
	else
	{
		for (auto& Pair : m_PartObjects)
		{
			if (nullptr != Pair.second)
				Pair.second->Update(fTimeDelta);
		}

		if (nullptr != m_pBody)
			Apply_RootMotion(m_pBody->Get_LastRootMotionDelta());
	}

}

void CPlayer::Late_Update(_float fTimeDelta)
{
	Update_WeaponHitboxes();

	for (auto& Pair : m_PartObjects)
	{
		if (nullptr != Pair.second)
			Pair.second->Late_Update(fTimeDelta);
	}

	if (nullptr != m_pCollider && nullptr != m_pTransformCom)
	{
		m_pCollider->Update(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()));
		m_pCollider->Register();
	}

	Update_SkillCollider();
}

HRESULT CPlayer::Render()
{
	return S_OK;
}

void CPlayer::Apply_RootMotion(const _float3& vLocalDelta)
{
	// delta 0이면 Skip
	if (0.f == vLocalDelta.x && 0.f == vLocalDelta.y && 0.f == vLocalDelta.z)
		return;

	_float3 vAdjustedLocal = vLocalDelta;
	if (true == Is_SkillF_KnightKiller_Start())
	{
		vAdjustedLocal.x *= m_fSkillFStartTravelScale;
		vAdjustedLocal.z *= m_fSkillFStartTravelScale;
	}

	// CTransform의 축을 정규화해 회전 기저만 추출
	_vector vRight = XMVector3Normalize(m_pTransformCom->Get_State(STATE::RIGHT));
	_vector vUp = XMVector3Normalize(m_pTransformCom->Get_State(STATE::UP));
	_vector vLook = XMVector3Normalize(m_pTransformCom->Get_State(STATE::LOOK));

	// 로컬 델타 -> 월드 델타 (회전만 적용)
	_vector vWorldDelta = XMVectorScale(vRight, vLocalDelta.x);
	vWorldDelta = XMVectorAdd(vWorldDelta, XMVectorScale(vUp, vLocalDelta.y));
	vWorldDelta = XMVectorAdd(vWorldDelta, XMVectorScale(vLook, vLocalDelta.z));

	// 현재 Position에 누적
	_vector vPos = m_pTransformCom->Get_State(STATE::POSITION);
	vPos = XMVectorAdd(vPos, vWorldDelta);

	_float3 vCandidatePosition = {};
	XMStoreFloat3(&vCandidatePosition, vPos);

	Try_ApplyMovementPosition(vCandidatePosition);
}

void CPlayer::Handle_ActionTransition(CHARACTER_ACTION eFromAction, CHARACTER_ACTION_STEP eFromStep,CHARACTER_ACTION eToAction, CHARACTER_ACTION_STEP eToStep, _bool bInitial)
{
	if (nullptr == m_pBody)
		return;

	m_pBody->Play_Action(eToAction, eToStep);

	const _bool bLeavingDash =
		(CHARACTER_ACTION::DASH == eFromAction || CHARACTER_ACTION::BACK_DASH == eFromAction) &&
		(CHARACTER_ACTION::DASH != eToAction && CHARACTER_ACTION::BACK_DASH != eToAction);

	if (true == bLeavingDash)
	{
		Resolve_BodyBlockOverlap();
	}

	if (CHARACTER_ACTION::SKILL_F == eFromAction
		&& CHARACTER_ACTION::SKILL_F != eToAction)
	{
		Enable_SkillCollider(false);
	}
}

void CPlayer::Face_DirectionImmediately(const _float3& vDirWorld)
{
	if (nullptr == m_pTransformCom)
		return;

	CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
	pTransform->Rotate_Toward_XZ(XMLoadFloat3(&vDirWorld), XM_PI);
}

CHARACTER_ACTION CPlayer::Pick_RunEndByFoot() const
{
	if (nullptr == m_pBody)
		return CHARACTER_ACTION::RUN_END_LEFT;

	return m_pBody->Pick_RunEndAction();
}

CHARACTER_ACTION CPlayer::Pick_RunFastVariant(const _float3& vMoveDirWorld, CHARACTER_ACTION eCurrent) const
{
	if (nullptr == m_pTransformCom)
		return CHARACTER_ACTION::RUN_FAST;

	_vector vLook = XMVector3Normalize(
		XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f));
	_vector vDir = XMVector3Normalize(
		XMVectorSetY(XMLoadFloat3(&vMoveDirWorld), 0.f));

	_float fDot = XMVectorGetX(XMVector3Dot(vLook, vDir));
	_float fCrossY = XMVectorGetY(XMVector3Cross(vLook, vDir));

	// Hysteresis: 진입 임계는 느슨, 이탈 임계는 빡빡 → 경계 토글링 방지
	constexpr _float fEnterCos = 0.866f;  // 30°  (FAST → LEFT/RIGHT)
	constexpr _float fExitCos = 0.966f;  // 15°  (LEFT/RIGHT → FAST)

	const _bool bInLean =
		(CHARACTER_ACTION::RUN_FAST_LEFT == eCurrent) ||
		(CHARACTER_ACTION::RUN_FAST_RIGHT == eCurrent);

	const _float fThreshold = bInLean ? fExitCos : fEnterCos;

	if (fDot >= fThreshold)
		return CHARACTER_ACTION::RUN_FAST;

	return (fCrossY > 0.f)
		? CHARACTER_ACTION::RUN_FAST_RIGHT
		: CHARACTER_ACTION::RUN_FAST_LEFT;
}

void CPlayer::Set_EquippedWeapon(EQUIPPED_WEAPON_ID eId)
{
	if (m_eEquippedWeapon == eId)
		return;

	m_eEquippedWeapon = eId;
	Apply_Loadout();

	if (nullptr != m_pBody)
	{
		m_pBody->Set_EquippedWeaponId(eId);

		const WEAPON_INFO* pInfo = Find_WeaponInfo(eId);
		const WEAPON_TYPE eType = (nullptr != pInfo) ? pInfo->eCategory : WEAPON_TYPE::DEFAULT;
		m_pBody->Set_WeaponType(eType);
	}
}

_bool CPlayer::Consume_DashCharge()
{
	if (m_iDashChargeCurrent <= 0)
		return false;

	--m_iDashChargeCurrent;

	return true;
}

void CPlayer::Set_WeaponsVisible(_bool bVisible)
{
	if (m_bWeaponsVisible == bVisible)
		return;

	m_bWeaponsVisible = bVisible;

	if (true == bVisible)
		m_fIdleTimer = 0.f;

	Refresh_WeaponVisibility();
}

void CPlayer::Tick_DashRegen(_float fTimeDelta)
{
	// 항상 일정 주기 - 풀 차지 여도 타이머는 리셋
	m_fDashRegenTimer += fTimeDelta;

	while (m_fDashRegenTimer >= m_fDashRegenInterval)
	{
		m_fDashRegenTimer -= m_fDashRegenInterval;

		if (m_iDashChargeCurrent < m_iDashChargeMax)
			++m_iDashChargeCurrent;
	}
}

void CPlayer::Tick_WeaponHideTimer(_float fTimeDelta)
{
	if (false == m_bWeaponsVisible || nullptr == m_pStateMachine)
	{
		m_fIdleTimer = 0.f;
		return;
	}

	if (CHARACTER_ACTION::IDLE != m_pStateMachine->Get_CurrentCharacterAction())
	{
		m_fIdleTimer = 0.f;
		return;
	}

	m_fIdleTimer += fTimeDelta;

	if (m_fIdleTimer >= m_fIdleThreshold)
	{
		m_fIdleTimer = 0.f;
		m_pStateMachine->Try_Action(CHARACTER_ACTION::UNDRAW);
	}
}

void CPlayer::Trigger_WeaponSwap()
{
	const EQUIPPED_WEAPON_ID eNext =
		(EQUIPPED_WEAPON_ID::KNIGHT_KILLER == m_eEquippedWeapon)
		? EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG
		: EQUIPPED_WEAPON_ID::KNIGHT_KILLER;

	Set_EquippedWeapon(eNext);
	Set_WeaponsVisible(true);

	m_fWeaponSwapCooldownTimer = WEAPON_SWAP_COOLDOWN;
}

void CPlayer::Trigger_SkillF()
{
	Set_WeaponsVisible(true);
	m_fSkillFCooldownTimer = SKILL_F_COOLDOWN;
}

void CPlayer::Tick_SkillCooldowns(_float fTimeDelta)
{
	if (m_fWeaponSwapCooldownTimer > 0.f)
		m_fWeaponSwapCooldownTimer = max(0.f, m_fWeaponSwapCooldownTimer - fTimeDelta);

	if (m_fSkillFCooldownTimer > 0.f)
		m_fSkillFCooldownTimer = max(0.f, m_fSkillFCooldownTimer - fTimeDelta);
}

HRESULT CPlayer::Ready_PartObjects()
{
	// Body 
	CBody_Player::BODY_PLAYER_DESC BodyDesc{};
	BodyDesc.pParentMatrix = m_pTransformCom->Get_WorldMatrixPtr();
	BodyDesc.pParentState = &m_iState;

	if (FAILED(__super::Add_PartObject(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_GameObject_Body_Player"),
		TEXT("Body"), &BodyDesc)))
		return E_FAIL;

	m_pBody = dynamic_cast<CBody_Player*>(m_PartObjects[TEXT("Body")]);
	Safe_AddRef(m_pBody);

	// Weapon Right
	CWeapon::WEAPON_DESC WeaponRDesc{};
	WeaponRDesc.pParentMatrix = m_pTransformCom->Get_WorldMatrixPtr();
	WeaponRDesc.pSocketBoneMatrix =	m_pBody->Get_BoneMatrixPtr("Prop_Weapon_Dualwield_01_R");
	WeaponRDesc.pModelPrototypeTag = TEXT("Prototype_Component_Model_Weapon_KnightKiller");
	WeaponRDesc.bInitiallyVisible = false;
	WeaponRDesc.eAttackGroup = COLLISION_GROUP::PLAYER_ATTACK;
	
	if (FAILED(__super::Add_PartObject(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_GameObject_Weapon"),
		TEXT("Weapon_R"), &WeaponRDesc)))
		return E_FAIL;

	m_pWeaponR = dynamic_cast<CWeapon*>(m_PartObjects[TEXT("Weapon_R")]);
	Safe_AddRef(m_pWeaponR);

	// Weapon Left
	CWeapon::WEAPON_DESC WeaponLDesc{};
	WeaponLDesc.pParentMatrix = m_pTransformCom->Get_WorldMatrixPtr();
	WeaponLDesc.pSocketBoneMatrix = m_pBody->Get_BoneMatrixPtr("Prop_Weapon_Dualwield_01_L");
	WeaponLDesc.pModelPrototypeTag = TEXT("Prototype_Component_Model_Weapon_KasakaVenomFang");
	WeaponLDesc.bInitiallyVisible = false;
	WeaponLDesc.eAttackGroup = COLLISION_GROUP::PLAYER_ATTACK;

	if (FAILED(__super::Add_PartObject(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_GameObject_Weapon"),
		TEXT("Weapon_L"), &WeaponLDesc)))
		return E_FAIL;

	m_pWeaponL = dynamic_cast<CWeapon*>(m_PartObjects[TEXT("Weapon_L")]);
	Safe_AddRef(m_pWeaponL);

	auto SetupWeaponHitCallback = [this](CWeapon* pWeapon)
		{
			if (nullptr == pWeapon)
				return;

			CCollider* pCollider = pWeapon->Get_BladeCollider();
			if (nullptr == pCollider)
				return;

			auto HitCallback = [this, pWeapon](CCollider* pOther)
				{
					On_WeaponHitEnter(pWeapon, pOther);
				};

			pCollider->Set_OnHitEnter(HitCallback);
			pCollider->Set_OnHitStay(HitCallback);
		};

	SetupWeaponHitCallback(m_pWeaponR);
	SetupWeaponHitCallback(m_pWeaponL);

	return S_OK;
}

HRESULT CPlayer::Ready_StateMachine()
{
	const CHARACTER_ANIM_TABLE_DESC* pAnimTable = Find_CharacterAnimTable(CHARACTER_TYPE::SUNGJINWOO_OVERDRIVE);
	if (nullptr == pAnimTable)
		return E_FAIL;

	m_pStateMachine = CPlayer_StateMachine::Create(pAnimTable);
	if (nullptr == m_pStateMachine)
		return E_FAIL;

	m_pStateMachine->Bind_Owner(this);

	if (nullptr != m_pBody)
		m_pBody->Set_Listener(m_pStateMachine);

	if (false == m_pStateMachine->Enter_InitialState(CHARACTER_ACTION::IDLE))
		return E_FAIL;

	return S_OK;
}

HRESULT CPlayer::Ready_Components(const PLAYER_DESC& Desc)
{
	CNavigationAgent::NAVIGATION_AGENT_DESC NavigationDesc{};
	NavigationDesc.pNavMesh = Desc.pNavMesh;
	NavigationDesc.iStartCellIndex = Desc.iStartCellIndex;

	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_NavigationAgent"),
		TEXT("Com_NavigationAgent"),
		reinterpret_cast<CComponent**>(&m_pNavigationAgent),
		&NavigationDesc)))
		return E_FAIL;

	if (nullptr != m_pNavigationAgent &&
		m_pNavigationAgent->Has_NavMesh() &&
		NAVMESH_INVALID_INDEX == m_pNavigationAgent->Get_CurrentCellIndex())
	{
		_float3 vPosition = {};
		XMStoreFloat3(&vPosition, m_pTransformCom->Get_State(STATE::POSITION));
		m_pNavigationAgent->Find_CurrentCell(vPosition);
	}

	// Body OBB Collider
	m_pCollider = CCollider::Create(m_pDevice, m_pContext);
	if (nullptr == m_pCollider)
		return E_FAIL;

	CCollider::COLLIDER_DESC ColliderDesc{};
	ColliderDesc.eBoundingType = COLLIDER::OBB;
	ColliderDesc.eGroup = COLLISION_GROUP::PLAYER_BODY;
	ColliderDesc.vCenter = _float3(0.f, 1.035f, 0.f);
	ColliderDesc.vSize = _float3(0.69f, 2.07f, 0.69f);
	ColliderDesc.vRadians = _float3(0.f, 0.f, 0.f);
	ColliderDesc.pOwner = this;

	if (FAILED(m_pCollider->Initialize(&ColliderDesc)))
		return E_FAIL;

	if (FAILED(Ready_SkillCollider()))
		return E_FAIL;

	return S_OK;
}

_bool CPlayer::Resolve_NavigationPosition(const _float3& vCandidatePosition, _float3* pOutPosition)
{
	if (nullptr == pOutPosition)
		return false;

	if (nullptr == m_pNavigationAgent || false == m_pNavigationAgent->Has_NavMesh())
	{
		*pOutPosition = vCandidatePosition;
		return true;
	}

	return m_pNavigationAgent->Try_Move(vCandidatePosition, pOutPosition);
}

_bool CPlayer::Resolve_BodyBlockingPosition(const _float3& vCurrentPosition, const _float3& vCandidatePosition, _float3* pOutPosition)
{
	if (nullptr == pOutPosition)
		return false;

	*pOutPosition = vCandidatePosition;

	if (nullptr == m_pGameInstance)
		return true;

	const map<const _wstring, CLayer*>* pLayers =
		m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));

	if (nullptr == pLayers)
		return true;

	auto iterLayer = pLayers->find(TEXT("Layer_Monster"));
	if (pLayers->end() == iterLayer || nullptr == iterLayer->second)
		return true;

	const CNavMesh* pNavMesh = nullptr;
	if (nullptr != m_pNavigationAgent && true == m_pNavigationAgent->Has_NavMesh())
		pNavMesh = m_pNavigationAgent->Get_NavMesh();

	_int CandidateCells[BODY_BLOCK_MAX_CANDIDATE_CELLS] = {};
	_uint iNumCandidateCells = 0;

	if (nullptr != pNavMesh)
	{
		const _int iCurrentCellIndex = pNavMesh->Find_Cell(vCurrentPosition);
		const _int iCandidateCellIndex = pNavMesh->Find_Cell(vCandidatePosition);

		Collect_BodyBlockCandidateCells(
			pNavMesh,
			iCurrentCellIndex,
			CandidateCells,
			&iNumCandidateCells);

		Collect_BodyBlockCandidateCells(
			pNavMesh,
			iCandidateCellIndex,
			CandidateCells,
			&iNumCandidateCells);
	}

	const _bool bUseCellFilter = (iNumCandidateCells > 0);

	constexpr _float BODY_BLOCK_SAFE_T = { 0.001f };
	constexpr _float BODY_BLOCK_QUERY_MARGIN = { 0.5f };

	const _float fMoveX = vCandidatePosition.x - vCurrentPosition.x;
	const _float fMoveZ = vCandidatePosition.z - vCurrentPosition.z;
	const _float fMoveLength = sqrtf(fMoveX * fMoveX + fMoveZ * fMoveZ);

	_float fBestT = 1.f;

	const list<CGameObject*>& MonsterObjects = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : MonsterObjects)
	{
		CMonster* pMonster = dynamic_cast<CMonster*>(pObject);
		if (nullptr == pMonster)
			continue;

		if (pMonster->Get_CurrentHP() <= 0.f)
			continue;

		// 임시로 꺼둠 - 몬스터 수 많아지면 주석 해제
		//if (true == bUseCellFilter)
		//{
		//	const _int iMonsterCellIndex = pMonster->Get_CurrentNavCellIndex();

		//	if (false == Contains_BodyBlockCandidateCell(
		//		CandidateCells,
		//		iNumCandidateCells,
		//		iMonsterCellIndex))
		//	{
		//		continue;
		//	}
		//}

		CTransform* pMonsterTransform = pMonster->Get_Transform();
		if (nullptr == pMonsterTransform)
			continue;

		_float3 vMonsterPosition = {};
		XMStoreFloat3(
			&vMonsterPosition,
			pMonsterTransform->Get_State(STATE::POSITION));

		const _float fCombinedRadius =
			PLAYER_BODY_BLOCK_RADIUS +
			Get_MonsterBodyBlockRadius(pMonster) +
			BODY_BLOCK_SKIN;

		const _float fQueryRadius =
			fCombinedRadius + fMoveLength + BODY_BLOCK_QUERY_MARGIN;

		const _float fToMonsterX = vMonsterPosition.x - vCurrentPosition.x;
		const _float fToMonsterZ = vMonsterPosition.z - vCurrentPosition.z;
		const _float fToMonsterSq =
			fToMonsterX * fToMonsterX + fToMonsterZ * fToMonsterZ;

		if (fToMonsterSq > fQueryRadius * fQueryRadius)
			continue;

		_float fHitT = 1.f;
		if (true == Clip_SegmentByCircleXZ(
			vCurrentPosition,
			vCandidatePosition,
			vMonsterPosition,
			fCombinedRadius,
			&fHitT))
		{
			if (fHitT < fBestT)
				fBestT = fHitT;
		}
	}

	if (fBestT < 1.f)
	{
		const _float fSafeT =
			(fBestT > BODY_BLOCK_SAFE_T) ? fBestT - BODY_BLOCK_SAFE_T : 0.f;

		pOutPosition->x =
			vCurrentPosition.x + (vCandidatePosition.x - vCurrentPosition.x) * fSafeT;

		pOutPosition->y =
			vCurrentPosition.y + (vCandidatePosition.y - vCurrentPosition.y) * fSafeT;

		pOutPosition->z =
			vCurrentPosition.z + (vCandidatePosition.z - vCurrentPosition.z) * fSafeT;
	}

	return true;
}

_bool CPlayer::Resolve_BodyOverlapPosition(const _float3& vPosition, _float3* pOutPosition) const
{
	if (nullptr == pOutPosition)
		return false;

	*pOutPosition = vPosition;

	if (nullptr == m_pGameInstance)
		return true;

	const map<const _wstring, CLayer*>* pLayers =
		m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));

	if (nullptr == pLayers)
		return true;

	auto iterLayer = pLayers->find(TEXT("Layer_Monster"));
	if (pLayers->end() == iterLayer || nullptr == iterLayer->second)
		return true;

	constexpr _uint  OVERLAP_SOLVE_ITERATIONS = 3;
	constexpr _float OVERLAP_PUSH_EPSILON = 0.01f;

	for (_uint i = 0; i < OVERLAP_SOLVE_ITERATIONS; ++i)
	{
		_bool bAdjusted = false;

		for (CGameObject* pObject : iterLayer->second->Get_GameObjects())
		{
			CMonster* pMonster = dynamic_cast<CMonster*>(pObject);
			if (nullptr == pMonster || pMonster->Get_CurrentHP() <= 0.f)
				continue;

			CTransform* pMonsterTransform = pMonster->Get_Transform();
			if (nullptr == pMonsterTransform)
				continue;

			_float3 vMonsterPosition{};
			XMStoreFloat3(&vMonsterPosition, pMonsterTransform->Get_State(STATE::POSITION));

			const _float fCombinedRadius =
				PLAYER_BODY_BLOCK_RADIUS +
				Get_MonsterBodyBlockRadius(pMonster) +
				BODY_BLOCK_SKIN;

			const _float fToPlayerX = pOutPosition->x - vMonsterPosition.x;
			const _float fToPlayerZ = pOutPosition->z - vMonsterPosition.z;
			const _float fDistSq = fToPlayerX * fToPlayerX + fToPlayerZ * fToPlayerZ;

			if (fDistSq >= fCombinedRadius * fCombinedRadius)
				continue;

			_float fDirX = 0.f;
			_float fDirZ = 1.f;

			if (fDistSq > BODY_BLOCK_MIN_MOVE_SQ)
			{
				const _float fInvDist = 1.f / sqrtf(fDistSq);
				fDirX = fToPlayerX * fInvDist;
				fDirZ = fToPlayerZ * fInvDist;
			}
			else if (nullptr != m_pTransformCom)
			{
				_vector vLook = XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f);
				const _float fLookLenSq = XMVectorGetX(XMVector3LengthSq(vLook));

				if (fLookLenSq > BODY_BLOCK_MIN_MOVE_SQ)
				{
					vLook = XMVectorScale(vLook, 1.f / sqrtf(fLookLenSq));
					fDirX = XMVectorGetX(vLook);
					fDirZ = XMVectorGetZ(vLook);
				}
			}

			const _float fPushRadius = fCombinedRadius + OVERLAP_PUSH_EPSILON;

			pOutPosition->x = vMonsterPosition.x + fDirX * fPushRadius;
			pOutPosition->z = vMonsterPosition.z + fDirZ * fPushRadius;

			bAdjusted = true;
		}

		if (false == bAdjusted)
			break;
	}

	return true;
}

void CPlayer::Resolve_BodyBlockOverlap()
{
	if (nullptr == m_pTransformCom)
		return;

	_float3 vCurrentPosition{};
	XMStoreFloat3(&vCurrentPosition, m_pTransformCom->Get_State(STATE::POSITION));

	_float3 vResolvedPosition{};
	if (false == Resolve_BodyOverlapPosition(vCurrentPosition, &vResolvedPosition))
		return;

	m_pTransformCom->Set_State(
		STATE::POSITION,
		XMVectorSetW(XMLoadFloat3(&vResolvedPosition), 1.f));

	if (nullptr != m_pNavigationAgent && true == m_pNavigationAgent->Has_NavMesh())
		m_pNavigationAgent->Find_CurrentCell(vResolvedPosition);
}

_bool CPlayer::Try_ApplyMovementPosition(const _float3& vCandidatePosition)
{
	if (nullptr == m_pTransformCom)
		return false;

	_float3 vCurrentPosition = {};
	XMStoreFloat3(&vCurrentPosition, m_pTransformCom->Get_State(STATE::POSITION));

	_float3 vResolvedPosition = {};
	if (false == Resolve_NavigationPosition(vCandidatePosition, &vResolvedPosition))
		return false;

	// 애니메이션에 Y좌표 변동이 있을경우 재생 후 다시 복원
	if (true == Is_AerialAction())
		vResolvedPosition.y = vCandidatePosition.y;

	if (BODY_BLOCK_POLICY::BLOCK == Get_BodyBlockPolicy())
	{
		_float3 vBodyBlockedPosition = {};
		if (true == Resolve_BodyBlockingPosition(
			vCurrentPosition,
			vResolvedPosition,
			&vBodyBlockedPosition))
		{
			vResolvedPosition = vBodyBlockedPosition;
		}

		_float3 vOverlapResolvedPosition = {};
		if (true == Resolve_BodyOverlapPosition(vResolvedPosition, &vOverlapResolvedPosition))
		{
			vResolvedPosition = vOverlapResolvedPosition;
		}
	}

	m_pTransformCom->Set_State(
		STATE::POSITION,
		XMVectorSetW(XMLoadFloat3(&vResolvedPosition), 1.f));

	if (nullptr != m_pNavigationAgent && true == m_pNavigationAgent->Has_NavMesh())
		m_pNavigationAgent->Find_CurrentCell(vResolvedPosition);

	return true;
}

BODY_BLOCK_POLICY CPlayer::Get_BodyBlockPolicy() const
{
	if (nullptr == m_pStateMachine)
		return BODY_BLOCK_POLICY::BLOCK;

	switch (m_pStateMachine->Get_CurrentCharacterAction())
	{
	case CHARACTER_ACTION::DASH:
	case CHARACTER_ACTION::BACK_DASH:
		return BODY_BLOCK_POLICY::PASS_THROUGH;
	default:
		return BODY_BLOCK_POLICY::BLOCK;
	}
}

_float CPlayer::Get_MonsterBodyBlockRadius(const CMonster* pMonster) const
{
	if (nullptr == pMonster)
		return 0.45f;

	switch (pMonster->Get_SpawnType())
	{
	case SPAWN_TYPE::MONSTER_BOSS:
		return 0.75f;

	case SPAWN_TYPE::MONSTER_ELITE:
		return 0.55f;

	case SPAWN_TYPE::MONSTER_NORMAL:
	default:
		return 0.45f;
	}
}

void CPlayer::Add_BodyBlockCandidateCell(_int* pCandidateCells, _uint* pNumCandidateCells, _int iCellIndex) const
{
	if (nullptr == pCandidateCells || nullptr == pNumCandidateCells)
		return;

	if (NAVMESH_INVALID_INDEX == iCellIndex)
		return;

	for (_uint i = 0; i < *pNumCandidateCells; ++i)
	{
		if (pCandidateCells[i] == iCellIndex)
			return;
	}

	if (*pNumCandidateCells >= BODY_BLOCK_MAX_CANDIDATE_CELLS)
		return;

	pCandidateCells[*pNumCandidateCells] = iCellIndex;
	++(*pNumCandidateCells);
}

_bool CPlayer::Contains_BodyBlockCandidateCell(const _int* pCandidateCells, _uint iNumCandidateCells, _int iCellIndex) const
{
	if (nullptr == pCandidateCells)
		return false;

	if (NAVMESH_INVALID_INDEX == iCellIndex)
		return false;

	for (_uint i = 0; i < iNumCandidateCells; ++i)
	{
		if (pCandidateCells[i] == iCellIndex)
			return true;
	}

	return false;
}

void CPlayer::Collect_BodyBlockCandidateCells(const CNavMesh* pNavMesh, _int iCellIndex, _int* pCandidateCells, _uint* pNumCandidateCells) const
{
	if (nullptr == pNavMesh)
		return;

	Add_BodyBlockCandidateCell(pCandidateCells, pNumCandidateCells, iCellIndex);

	const CCell* pCell = pNavMesh->Get_Cell(iCellIndex);
	if (nullptr == pCell)
		return;

	for (_uint i = 0; i < 3; ++i)
	{
		Add_BodyBlockCandidateCell(
			pCandidateCells,
			pNumCandidateCells,
			pCell->Get_NeighborIndex(i));
	}
}

_bool CPlayer::Clip_SegmentByCircleXZ(const _float3& vCurrentPosition, const _float3& vCandidatePosition, const _float3& vCircleCenter, _float fRadius, _float* pOutT) const
{
	if (nullptr == pOutT)
		return false;

	constexpr _float BODY_BLOCK_MIN_MOVE_SQ = { 0.000001f };

	const _float fDeltaX = vCandidatePosition.x - vCurrentPosition.x;
	const _float fDeltaZ = vCandidatePosition.z - vCurrentPosition.z;

	const _float fA = fDeltaX * fDeltaX + fDeltaZ * fDeltaZ;
	if (fA <= BODY_BLOCK_MIN_MOVE_SQ)
		return false;

	const _float fStartX = vCurrentPosition.x - vCircleCenter.x;
	const _float fStartZ = vCurrentPosition.z - vCircleCenter.z;
	const _float fEndX = vCandidatePosition.x - vCircleCenter.x;
	const _float fEndZ = vCandidatePosition.z - vCircleCenter.z;

	const _float fRadiusSq = fRadius * fRadius;
	const _float fStartDistSq = fStartX * fStartX + fStartZ * fStartZ;
	const _float fEndDistSq = fEndX * fEndX + fEndZ * fEndZ;

	if (fStartDistSq <= fRadiusSq)
	{
		if (fEndDistSq < fStartDistSq)
		{
			*pOutT = 0.f;
			return true;
		}

		return false;
	}

	const _float fB = 2.f * (fStartX * fDeltaX + fStartZ * fDeltaZ);
	const _float fC = fStartDistSq - fRadiusSq;
	const _float fDiscriminant = fB * fB - 4.f * fA * fC;

	if (fDiscriminant < 0.f)
		return false;

	const _float fSqrtD = sqrtf(fDiscriminant);
	const _float fT = (-fB - fSqrtD) / (2.f * fA);

	if (fT < 0.f || fT > 1.f)
		return false;

	*pOutT = fT;
	return true;
}

void CPlayer::Gather_RawInput(PLAYER_RAW_INPUT_FRAME* pOutRaw)
{
	if (nullptr == pOutRaw)
		return;

	pOutRaw->bRButtonHeld = (m_pGameInstance->Get_MouseBtnState(MOUSEBTN::RBUTTON) & 0x80) != 0;
	if (false == pOutRaw->bRButtonHeld)
	{
		pOutRaw->bMoveForwardHeld = (m_pGameInstance->Get_KeyState('W') & 0x80) != 0;
		pOutRaw->bMoveBackwardHeld = (m_pGameInstance->Get_KeyState('S') & 0x80) != 0;
		pOutRaw->bMoveLeftHeld = (m_pGameInstance->Get_KeyState('A') & 0x80) != 0;
		pOutRaw->bMoveRightHeld = (m_pGameInstance->Get_KeyState('D') & 0x80) != 0;
	}
	pOutRaw->bDashPressed = m_pGameInstance->Get_KeyDown(VK_SPACE);
	pOutRaw->bWeaponSwapPressed = m_pGameInstance->Get_KeyDown('C');
	pOutRaw->bSkillFPressed = m_pGameInstance->Get_KeyDown('F');

	const _bool bMouseLBtnDown = m_pGameInstance->Get_MouseBtnDown(MOUSEBTN::LBUTTON);
	const _bool bMouseLBtnHeld = m_pGameInstance->Get_MouseBtnState(MOUSEBTN::LBUTTON);
	pOutRaw->bLButtonPressed = bMouseLBtnDown || bMouseLBtnHeld;

	pOutRaw->lMouseDeltaX = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::X);
	pOutRaw->lMouseDeltaY = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::Y);
}

void CPlayer::Apply_MoveIntent(const PLAYER_INTENT_FRAME& Intent, _float fTimeDelta)
{
	if (nullptr != m_pStateMachine)
	{
		const CHARACTER_ACTION eCurrent = m_pStateMachine->Get_CurrentCharacterAction();

		const _bool bIsLocomotion =
			(CHARACTER_ACTION::IDLE == eCurrent) ||
			(CHARACTER_ACTION::WALK == eCurrent) ||
			(CHARACTER_ACTION::RUN == eCurrent) ||
			(CHARACTER_ACTION::RUN_FAST == eCurrent) ||
			(CHARACTER_ACTION::RUN_FAST_LEFT == eCurrent) ||
			(CHARACTER_ACTION::RUN_FAST_RIGHT == eCurrent);


		if (false == bIsLocomotion)
			return;
	}

	if (false == Has_MoveIntent(Intent))
		return;


	CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);

	_vector vDirWorld = XMLoadFloat3(&Intent.vMoveDirWorld);

	// 현재 Look (XZ) 와 목표 방향 사이 각도 차이 계산
	_vector vLookXZ = XMVector3Normalize(XMVectorSetY(pTransform->Get_State(STATE::LOOK), 0.f));
	_vector vTargetXZ = XMVector3Normalize(XMVectorSetY(vDirWorld, 0.f));
	_float fDot = XMVectorGetX(XMVector3Dot(vLookXZ, vTargetXZ));
	fDot = max(-1.f, min(1.f, fDot));
	const _float fAngleDeg = XMConvertToDegrees(acosf(fDot));

	// 각도 의존 회전 속도: 작은 각=느림(lean 가시), 큰 각=빠름(스냅)
	constexpr _float fSlowDeg = 30.f;        // 이하: 느린 회전
	constexpr _float fSnapDeg = 135.f;       // 이상: 최대 회전 (스냅)
	constexpr _float fSlowRotDeg = 540.f;    // 느린 회전 속도
	const _float fMaxRotDeg = XMConvertToDegrees(pTransform->Get_RotationPerSec()); // Desc 값 (1440 권장)

	_float fRotDeg = fSlowRotDeg;
	if (fAngleDeg >= fSnapDeg)
	{
		fRotDeg = fMaxRotDeg;
	}
	else if (fAngleDeg > fSlowDeg)
	{
		const _float t = (fAngleDeg - fSlowDeg) / (fSnapDeg - fSlowDeg);
		fRotDeg = fSlowRotDeg + (fMaxRotDeg - fSlowRotDeg) * t;
	}

	const _float fStepRad = XMConvertToRadians(fRotDeg) * fTimeDelta;
	pTransform->Rotate_Toward_XZ(vDirWorld, fStepRad);

	// 회전 후 갱신된 Look 으로 이동
	vLookXZ = XMVector3Normalize(XMVectorSetY(pTransform->Get_State(STATE::LOOK), 0.f));
	const _float fSpeed = pTransform->Get_SpeedPerSec() * m_fSpeedCoeff;
	_vector vPos = pTransform->Get_State(STATE::POSITION);
	vPos = XMVectorAdd(vPos, XMVectorScale(vLookXZ, fSpeed * fTimeDelta));

	_float3 vCandidatePosition = {};
	XMStoreFloat3(&vCandidatePosition, vPos);

	Try_ApplyMovementPosition(vCandidatePosition);
}

_float CPlayer::Query_CameraYaw() const
{
	// VIEW 회전부 = 카메라 World 회전부의 Transpose
	// 카메라 Look (월드) = (View._13, View._23, View._33)
	const _float4x4* pView = m_pGameInstance->Get_Transform(D3DTS::VIEW);
	if (nullptr == pView)
		return 0.f;

	return atan2f(pView->_13, pView->_33);
}

void CPlayer::Apply_Loadout()
{
	if (nullptr == m_pWeaponR || nullptr == m_pWeaponL)
		return;

	const WEAPON_INFO* pInfo = Find_WeaponInfo(m_eEquippedWeapon);
	if (nullptr == pInfo)
		return;

	const _tchar* pRightModel = nullptr;
	const _tchar* pLeftModel = nullptr;
	_bool bLeftVisible = true;

	if (EQUIPPED_WEAPON_ID::NONE == m_eEquippedWeapon)
	{
		// DEFAULT - 양손 디폴트 단검 풀
		pRightModel = DAGGER_POOL[0];		// KnightKiller
		pLeftModel = DAGGER_POOL[1];		// KasakaVenomFang
	}
	else if (WEAPON_TYPE::DAGGER == pInfo->eCategory)
	{
		// 단검 장착
		pRightModel = pInfo->pModelTag;
		for (const _tchar* pCandidate : DAGGER_POOL)
		{
			if (0 != lstrcmpW(pCandidate, pRightModel))
			{
				pLeftModel = pCandidate;
				break;
			}
		}
	}
	else
	{
		// DAGGER 무기 아닌 단일 무기 경우
		pRightModel = pInfo->pModelTag;
		bLeftVisible = false;
	}

	if (nullptr != pRightModel)
		m_pWeaponR->Set_Model(pRightModel);

	if (nullptr != pLeftModel)
		m_pWeaponL->Set_Model(pLeftModel);

	m_bLeftVisibleFromLoadOut = bLeftVisible;
	Refresh_WeaponVisibility();
}

void CPlayer::Refresh_WeaponVisibility()
{
	if (nullptr != m_pWeaponR)
		m_pWeaponR->Set_Visible(m_bWeaponsVisible);
	if (nullptr != m_pWeaponL)
		m_pWeaponL->Set_Visible(m_bWeaponsVisible && m_bLeftVisibleFromLoadOut);
}

void CPlayer::Update_WeaponHitboxes()
{
	const _bool bHitboxActive =
		(nullptr != m_pStateMachine) &&
		m_pStateMachine->Is_AttackHitboxActive();

	const _uint iHitboxWindowSerial =
		(nullptr != m_pStateMachine) ?
		m_pStateMachine->Get_AttackHitboxWindowSerial() :
		m_iPrevAttackHitboxWindowSerial;

	if (iHitboxWindowSerial != m_iPrevAttackHitboxWindowSerial)
	{
		m_AttackHitTargets.clear();
		m_iPrevAttackHitboxWindowSerial = iHitboxWindowSerial;
	}

	if (nullptr != m_pWeaponR)
		m_pWeaponR->Set_AttackHitboxActive(bHitboxActive);

	if (nullptr != m_pWeaponL)
		m_pWeaponL->Set_AttackHitboxActive(bHitboxActive);

	if (false == bHitboxActive && true == m_bPrevAttackHitboxActive)
		m_AttackHitTargets.clear();

	m_bPrevAttackHitboxActive = bHitboxActive;
}

void CPlayer::Enter_FloatReaction(CHARACTER_ACTION eFloatAction)
{
	if (nullptr == m_pStateMachine)
		return;

	m_AttackHitTargets.clear();
	m_bPrevAttackHitboxActive = false;

	if (nullptr != m_pWeaponR)
		m_pWeaponR->Set_AttackHitboxActive(false);

	if (nullptr != m_pWeaponL)
		m_pWeaponL->Set_AttackHitboxActive(false);

	m_pStateMachine->Enter_FloatReaction(eFloatAction);
}

void CPlayer::Enable_SkillCollider(_bool bEnable)
{
	if (true == bEnable && false == m_bSkillColliderActive)
		m_SkillHitTargets.clear();

	m_bSkillColliderActive = bEnable;
}

void CPlayer::Set_SkillColliderRadius(_float fRadius)
{
	if (nullptr == m_pSkillCollider)
		return;

	m_pSkillCollider->Set_Radius(fRadius);
	m_fSkillColliderRadius = fRadius;
}

void CPlayer::On_WeaponHitEnter(CWeapon* pSourceWeapon, CCollider* pOther)
{
	if (nullptr == pSourceWeapon || nullptr == pOther)
		return;

	if (COLLISION_GROUP::MONSTER_BODY != pOther->Get_Group())
		return;

	CGameObject* pTarget = pOther->Get_Owner();
	if (nullptr == pTarget)
		return;

	auto key = std::make_pair(pSourceWeapon, pTarget);
	if (m_AttackHitTargets.end() != m_AttackHitTargets.find(key))
		return;

	m_AttackHitTargets.insert(key);

	CMonster* pMonster = dynamic_cast<CMonster*>(pTarget);
	if (nullptr == pMonster)
		return;

	pMonster->Take_Damage(10.f);
}

const WEAPON_INFO* CPlayer::Find_WeaponInfo(EQUIPPED_WEAPON_ID eId)
{
	for (const auto& Info : s_WeaponInfo)
	{
		if (Info.eId == eId)
			return &Info;
	}
	return nullptr;
}

_bool CPlayer::Is_AerialAction() const
{
	if (nullptr == m_pStateMachine)
		return false;

	const CHARACTER_ACTION eCur = m_pStateMachine->Get_CurrentCharacterAction();

	if (CHARACTER_ACTION::SKILL_F == eCur &&
		CHARACTER_ACTION_STEP::NONE == m_pStateMachine->Get_CurrentCharacterStep() &&
		EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG == m_eEquippedWeapon)
		return true;

	return false;
}

HRESULT CPlayer::Ready_SkillCollider()
{
	m_pSkillCollider = CCollider::Create(m_pDevice, m_pContext);
	if (nullptr == m_pSkillCollider)
		return E_FAIL;

	CCollider::COLLIDER_DESC Desc{};
	Desc.eBoundingType = COLLIDER::SPHERE;
	Desc.eGroup = COLLISION_GROUP::PLAYER_ATTACK;  
	Desc.vCenter = _float3(0.f, 0.9f, 0.f);         
	Desc.vSize = _float3(m_fSkillColliderRadius, 0.f, 0.f);  
	Desc.vRadians = _float3(0.f, 0.f, 0.f);
	Desc.pOwner = this;

	if (FAILED(m_pSkillCollider->Initialize(&Desc)))
	{
		Safe_Release(m_pSkillCollider);
		return E_FAIL;
	}

	m_pSkillCollider->Set_OnHitEnter([this](CCollider* pOther)
		{
			On_SkillColliderHit(pOther);
		});

	m_pSkillCollider->Set_OnHitStay([this](CCollider* pOther)
		{
			On_SkillColliderHit(pOther);
		});
}

void CPlayer::Update_SkillCollider()
{
	if (nullptr == m_pSkillCollider || nullptr == m_pTransformCom)
		return;

	if (false == m_bSkillColliderActive)
		return;

	// Player Transform 위치 + LOOK 방향 forward offset 으로 sphere 월드 위치 계산
	_vector vLook = XMVector3Normalize(
		XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f));
	_vector vPos = m_pTransformCom->Get_State(STATE::POSITION);
	_vector vSpherePos = XMVectorAdd(vPos, XMVectorScale(vLook, m_fSkillColliderForwardOffset));

	_matrix mWorld = XMMatrixIdentity();
	mWorld.r[3] = XMVectorSetW(vSpherePos, 1.f);

	m_pSkillCollider->Update(mWorld);
	m_pSkillCollider->Register();
}

void CPlayer::On_SkillColliderHit(CCollider* pOther)
{
	if (false == m_bSkillColliderActive)
		return;
	if (nullptr == m_pStateMachine || nullptr == pOther)
		return;

	// R6-B: SKILL_F + START + KnightKiller — 즉시 Start→Loop 전이 (damage X)
	if (true == Is_SkillF_KnightKiller_Start())
	{
		m_pStateMachine->Try_Action_External(
			CHARACTER_ACTION::SKILL_F, CHARACTER_ACTION_STEP::LOOP);

		Enable_SkillCollider(false);
		return;
	}

	if (m_fSkillColliderDamage <= 0.f)
		return;

	if (COLLISION_GROUP::MONSTER_BODY != pOther->Get_Group())
		return;

	CGameObject* pTarget = pOther->Get_Owner();
	if (nullptr == pTarget)
		return;

	if (m_SkillHitTargets.end() != m_SkillHitTargets.find(pTarget))
		return;
	m_SkillHitTargets.insert(pTarget);

	CMonster* pMonster = dynamic_cast<CMonster*>(pTarget);
	if (nullptr == pMonster)
		return;

	pMonster->Take_Damage(m_fSkillColliderDamage);
}

_bool CPlayer::Is_SkillF_KnightKiller_Start() const
{
	if (nullptr == m_pStateMachine)
		return false;

	return (CHARACTER_ACTION::SKILL_F == m_pStateMachine->Get_CurrentCharacterAction()
		&& CHARACTER_ACTION_STEP::START == m_pStateMachine->Get_CurrentCharacterStep()
		&& EQUIPPED_WEAPON_ID::KNIGHT_KILLER == m_eEquippedWeapon);
}

_bool CPlayer::Is_SkillF_KnightKiller_Loop() const
{
	if (nullptr == m_pStateMachine)
		return false;

	return (CHARACTER_ACTION::SKILL_F == m_pStateMachine->Get_CurrentCharacterAction()
		&& CHARACTER_ACTION_STEP::LOOP == m_pStateMachine->Get_CurrentCharacterStep()
		&& EQUIPPED_WEAPON_ID::KNIGHT_KILLER == m_eEquippedWeapon);
}

CMonster* CPlayer::Find_Target(_float fSearchRadius, _float fConeAngleDegrees) const
{
	if (nullptr == m_pTransformCom || nullptr == m_pGameInstance)
		return nullptr;
	if (fSearchRadius <= 0.f)
		return nullptr;

	const map<const _wstring, CLayer*>* pLayers =
		m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(TEXT("Layer_Monster"));
	if (pLayers->end() == iterLayer || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& MonsterObjects = iterLayer->second->Get_GameObjects();

	const _float fSearchRadiusSq = fSearchRadius * fSearchRadius;
	const _bool  bUseConeFilter = (fConeAngleDegrees < 360.f);
	const _float fHalfAngleCos = bUseConeFilter
		? cosf(XMConvertToRadians(fConeAngleDegrees * 0.5f))
		: -1.f;

	_float3 vPlayerPos = {};
	XMStoreFloat3(&vPlayerPos, m_pTransformCom->Get_State(STATE::POSITION));

	_float3 vPlayerForward = {};
	XMStoreFloat3(&vPlayerForward, XMVector3Normalize(
		XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f)));

	CMonster* pBest = nullptr;
	_float    fBestDistSq = fSearchRadiusSq;

	for (CGameObject* pObject : MonsterObjects)
	{
		CMonster* pMonster = dynamic_cast<CMonster*>(pObject);
		if (nullptr == pMonster) continue;
		if (pMonster->Get_CurrentHP() <= 0.f) continue;

		CTransform* pMonsterTransform = pMonster->Get_Transform();
		if (nullptr == pMonsterTransform) continue;

		_float3 vMonsterPos = {};
		XMStoreFloat3(&vMonsterPos, pMonsterTransform->Get_State(STATE::POSITION));

		const _float fDX = vMonsterPos.x - vPlayerPos.x;
		const _float fDZ = vMonsterPos.z - vPlayerPos.z;
		const _float fDistSq = fDX * fDX + fDZ * fDZ;

		if (fDistSq > fSearchRadiusSq) continue;
		if (fDistSq < 1e-4f)           continue;

		if (true == bUseConeFilter)
		{
			const _float fDist = sqrtf(fDistSq);
			const _float fDotForward =
				(fDX * vPlayerForward.x + fDZ * vPlayerForward.z) / fDist;

			if (fDotForward < fHalfAngleCos) continue;
		}

		if (fDistSq < fBestDistSq)
		{
			fBestDistSq = fDistSq;
			pBest = pMonster;
		}
	}
	return pBest;
}

void CPlayer::Teleport_BehindTarget(CMonster* pTarget)
{
	if (nullptr == pTarget || nullptr == m_pTransformCom)
		return;

	CTransform* pMonsterTransform = pTarget->Get_Transform();
	if (nullptr == pMonsterTransform)
		return;

	_float3 vMonsterPos = {};
	XMStoreFloat3(&vMonsterPos, pMonsterTransform->Get_State(STATE::POSITION));

	_float3 vMonsterBack = {};
	XMStoreFloat3(&vMonsterBack, XMVector3Normalize(
		XMVectorSetY(
			XMVectorNegate(pMonsterTransform->Get_State(STATE::LOOK)),
			0.f)));

	constexpr _float EXTRA_OFFSET = 0.5f;
	const _float fBackDistance =
		PLAYER_BODY_BLOCK_RADIUS +
		Get_MonsterBodyBlockRadius(pTarget) +
		EXTRA_OFFSET;

	const _float3 vBehindPos = {
		vMonsterPos.x + vMonsterBack.x * fBackDistance,
		vMonsterPos.y,
		vMonsterPos.z + vMonsterBack.z * fBackDistance
	};

	m_pTransformCom->Set_State(STATE::POSITION,
		XMVectorSet(vBehindPos.x, vBehindPos.y, vBehindPos.z, 1.f));

	_float3 vToMonsterDir = {
		vMonsterPos.x - vBehindPos.x,
		0.f,
		vMonsterPos.z - vBehindPos.z
	};
	Face_DirectionImmediately(vToMonsterDir);
}

CPlayer* CPlayer::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CPlayer* pInstance = new CPlayer(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CPlayer");
		Safe_Release(pInstance);
	}

	return pInstance;
}


CGameObject* CPlayer::Clone(void* pArg)
{
	CPlayer* pInstance = new CPlayer(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CPlayer");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CPlayer::Free()
{
	__super::Free();

	if (nullptr != m_pWeaponR && nullptr != m_pWeaponR->Get_BladeCollider())
		m_pWeaponR->Get_BladeCollider()->Clear_Callbacks();

	if (nullptr != m_pWeaponL && nullptr != m_pWeaponL->Get_BladeCollider())
		m_pWeaponL->Get_BladeCollider()->Clear_Callbacks();

	m_AttackHitTargets.clear();

	if (nullptr != m_pCollider)
		m_pCollider->Clear_Callbacks();
	Safe_Release(m_pCollider);

	if (nullptr != m_pSkillCollider)
		m_pSkillCollider->Clear_Callbacks();
	Safe_Release(m_pSkillCollider);

	Safe_Release(m_pNavigationAgent);
	Safe_Release(m_pStateMachine);
	Safe_Release(m_pIntentResolver);
	Safe_Release(m_pWeaponL);
	Safe_Release(m_pWeaponR);
	Safe_Release(m_pBody);
}
