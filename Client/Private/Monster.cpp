#include "Monster.h"
#include "GameInstance.h"
#include "NavigationAgent.h"
#include "NavMesh.h"
#include "PartObject.h"
#include "Body_Monster.h"
#include "Weapon.h"
#include "Collider.h"
#include "Monster_StateMachine.h"
#include "MonsterAnimTable.h"
#include "Layer.h"
#include "Player.h"
#include "HUD_GamePlay.h"
#include "DamageFont.h"
#include "WeaponTrailEffect.h"

CMonster::CMonster(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CContainerObject{ pDevice, pContext }
{
}

CMonster::CMonster(const CMonster& Prototype)
    : CContainerObject{ Prototype }
{
}

_int CMonster::Get_CurrentNavCellIndex() const
{
    if (nullptr == m_pNavigationAgent)
        return INVALID_INDEX;

    return  m_pNavigationAgent->Get_CurrentCellIndex();
}

void CMonster::Force_Break()
{
    if (m_fCurrentHP <= 0.f || false == m_bHasBreak || nullptr == m_pStateMachine)
        return;

    const MONSTER_ACTION eCur = m_pStateMachine->Get_CurrentMonsterAction();

    if (MONSTER_ACTION::CRASH == eCur || MONSTER_ACTION::DEATH == eCur)
        return;

    m_bPreserveBreakOnCrashExit = true;

    OutputDebugStringA("[Monster] FORCE BREAK (parry) -> CRASH (gauge preserved)\n");

    m_pStateMachine->Try_Action(MONSTER_ACTION::CRASH, MONSTER_ACTION_STEP::START);
}

void CMonster::Take_Damage(_float fAmount)
{
    if (m_fCurrentHP <= 0.f)
        return;

    auto frand01 = []() { return static_cast<_float>(rand()) / static_cast<_float>(RAND_MAX); };
    const _bool bCrit = (frand01() < 0.25f);
    fAmount *= (0.8f + frand01() * 0.4f);
    if (bCrit)
        fAmount *= 2.f;

    if (CHUD_GamePlay* pHUD = CHUD_GamePlay::Get_Instance())
        pHUD->Notify_ComboHit();

    _float fHPDamage = 0.f;
    _float fBreakDamage = 0.f;

    if (!m_bHasBreak)
    {
        fHPDamage = fAmount;
    }
    else if (m_fCurrentBreak > 0.f)
    {
        fHPDamage = fAmount * 0.7f;
        fBreakDamage = fAmount * 1.5f;
    }
    else
    {
        fHPDamage = fAmount * 1.5f;
    }

    _float fPrevBreak = m_fCurrentBreak;
    m_fCurrentBreak = max(0.f, m_fCurrentBreak - fBreakDamage);
    m_fCurrentHP = max(0.f, m_fCurrentHP - fHPDamage);

    if (CDamageFont* pDF = CDamageFont::Get_Instance())
    {
        _float3 vCenter;
        XMStoreFloat3(&vCenter, m_pTransformCom->Get_State(STATE::POSITION));
        vCenter.y += 1.2f;
        pDF->Spawn(vCenter, static_cast<_int>(fHPDamage + 0.5f), bCrit);
    }

    char szLog[192] = {};
    sprintf_s(szLog,
        "[Monster] DMG in=%.1f | HP -%.1f => %.1f/%.1f | Break -%.1f => %.1f/%.1f\n",
        fAmount,
        fHPDamage, m_fCurrentHP, m_fMaxHP,
        fBreakDamage, m_fCurrentBreak, m_fMaxBreak);
    OutputDebugStringA(szLog);

    if (true == m_bHasBreak && fPrevBreak > 0.f && m_fCurrentBreak <= 0.f)
    {
        OutputDebugStringA("[Monster] Break BROKEN -> CRASH\n");
        if (nullptr != m_pStateMachine)
            m_pStateMachine->Try_Action(MONSTER_ACTION::CRASH, MONSTER_ACTION_STEP::START);
    }

    if (m_fCurrentHP <= 0.f)
    {
        OutputDebugStringA("[Monster] HP DEPLETED -> DEATH\n");

        if (nullptr != m_pStateMachine)
            m_pStateMachine->Try_Action(MONSTER_ACTION::DEATH, MONSTER_ACTION_STEP::NONE);

        if (CHUD_GamePlay* pHUD = CHUD_GamePlay::Get_Instance())
            pHUD->Notify_Death(this);

        return;
    }

    if (CHUD_GamePlay* pHUD = CHUD_GamePlay::Get_Instance())
        pHUD->Notify_Hit(this);
}

void CMonster::Handle_ActionTransition(MONSTER_ACTION eFromAction, MONSTER_ACTION_STEP eFromStep, MONSTER_ACTION eToAction, MONSTER_ACTION_STEP eToStep, _bool bInitial)
{
    if (nullptr == m_pBody)
        return;

    if (MONSTER_ACTION::CRASH != eFromAction && MONSTER_ACTION::DEATH != eFromAction &&
        (MONSTER_ACTION::CRASH == eToAction || MONSTER_ACTION::DEATH == eToAction))
    {
        Set_WeaponHitboxActive(false);
    }

    if (false == bInitial &&
        MONSTER_ACTION::CRASH != eFromAction &&
        MONSTER_ACTION::CRASH == eToAction)
    {
        if (CHUD_GamePlay* pHUD = CHUD_GamePlay::Get_Instance())
            pHUD->Notify_Crash();
    }

    if (MONSTER_ACTION::DEATH != eFromAction && MONSTER_ACTION::DEATH == eToAction)
    {
        if (nullptr != m_pBody)
            m_pBody->Start_Dissolve(1.4f, _float4(1.35f, 0.03f, 0.02f, 1.f));

        if (CHUD_GamePlay* pHUD = CHUD_GamePlay::Get_Instance())
            pHUD->Notify_Death(this);
    }

    if (true == m_bHasBreak &&
        MONSTER_ACTION::CRASH != eFromAction &&
        MONSTER_ACTION::CRASH == eToAction)
    {
        m_fCrashDurationCurrent = m_fCrashDurationMax;
    }

    if (true == m_bHasBreak &&
        MONSTER_ACTION::CRASH == eFromAction &&
        MONSTER_ACTION::CRASH != eToAction)
    {
        if (false == m_bPreserveBreakOnCrashExit)
            m_fCurrentBreak = m_fMaxBreak;

        m_bPreserveBreakOnCrashExit = false;
        m_fCrashDurationCurrent = 0.f;
        
    }

    m_pBody->Play_Action(eToAction, eToStep, MONSTER_PHASE::COMMON);
}

void CMonster::Set_WeaponHitboxActive(_bool bActive)
{
    if (true == bActive)
        m_AttackHitTargets.clear();

    if (nullptr != m_pWeapon)
        m_pWeapon->Set_AttackHitboxActive(bActive);

    if (false == bActive)
        m_AttackHitTargets.clear();
}

void CMonster::Set_WeaponTrailActive(_bool bActive)
{
    if (nullptr != m_pWeaponTrail)
        m_pWeaponTrail->Set_Active(bActive);
}
void CMonster::On_AttackHitboxNotify(_bool bActive)
{
    Set_WeaponHitboxActive(bActive);
}

#ifdef _DEBUG
void CMonster::Debug_TryAction(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep)
{
    if (nullptr == m_pStateMachine)
        return;

    m_pStateMachine->Try_Action(eAction, eStep);
}
#endif

HRESULT CMonster::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CMonster::Initialize(void* pArg)
{
    MONSTER_DESC Desc{};

    if (nullptr != pArg)
        Desc = *static_cast<MONSTER_DESC*>(pArg);

    if (SPAWN_TYPE::END == Desc.eSpawnType)
        Desc.eSpawnType = Get_DefaultSpawnType();

    if (MONSTER_ANIM_SET::NONE == Desc.eAnimSet)
        Desc.eAnimSet = Get_DefaultAnimSet();

    if (nullptr == Desc.pBodyModelPrototypeTag)
        Desc.pBodyModelPrototypeTag = Get_DefaultBodyModelPrototypeTag();

    if (nullptr == Desc.pWeaponModelPrototypeTag)
        Desc.pWeaponModelPrototypeTag = Get_DefaultWeaponModelPrototypeTag();

    if (nullptr == Desc.pWeaponSocketBoneName)
        Desc.pWeaponSocketBoneName = Get_DefaultWeaponSocketBoneName();

    if (0.f == Desc.fMaxHP)
        Desc.fMaxHP = 1.f;

    if (0.f == Desc.fMaxBreak)
        Desc.fMaxBreak = 1.f;

    m_strName = Get_DefaultName();
    m_strTag = Get_DefaultName();

    m_eSpawnType = Desc.eSpawnType;
    m_eAnimSet = Desc.eAnimSet;

    m_fMaxHP = Desc.fMaxHP;
    m_fCurrentHP = Desc.fMaxHP;

    m_bHasBreak = Desc.bHasBreak;
    m_fMaxBreak = Desc.fMaxBreak;
    m_fCurrentBreak = Desc.fMaxBreak;

    m_iLevel = Desc.iLevel;
    m_strDisplayName = (Desc.szDisplayName[0] != 0) ? Desc.szDisplayName : TEXT("정보 없음");

    Set_Target(Desc.pTarget);

    if (FAILED(__super::Initialize(&Desc)))
        return E_FAIL;

    if (FAILED(Ready_Components(Desc)))
        return E_FAIL;

    if (FAILED(Ready_PartObjects(Desc)))
        return E_FAIL;

    if (FAILED(Ready_WeaponTrailEffect()))
        return E_FAIL;

    if (FAILED(Ready_StateMachine()))
        return E_FAIL;

    return S_OK;
}

void CMonster::Priority_Update(_float fTimeDelta)
{
    for (auto& Pair : m_PartObjects)
    {
        if (nullptr != Pair.second)
            Pair.second->Priority_Update(fTimeDelta);
    }
}

void CMonster::Update(_float fTimeDelta)
{
    Tick_AI(fTimeDelta);

    if (nullptr != m_pStateMachine)
        m_pStateMachine->Update(fTimeDelta);

    for (auto& Pair : m_PartObjects)
    {
        if (nullptr != Pair.second)
            Pair.second->Update(fTimeDelta);
    }

    if (nullptr != m_pBody)
        Apply_RootMotion(m_pBody->Get_LastRootMotionDelta());

    if (true == m_bHasBreak && m_fCrashDurationCurrent > 0.f)
    {
        m_fCrashDurationCurrent -= fTimeDelta;
        if (m_fCrashDurationCurrent <= 0.f)
        {
            m_fCrashDurationCurrent = 0.f;
            if (nullptr != m_pStateMachine)
            {
                m_pStateMachine->On_ActionFinished();
                m_pStateMachine->Try_Action(MONSTER_ACTION::IDLE, MONSTER_ACTION_STEP::NONE);
            }
        }
    }
}

void CMonster::Late_Update(_float fTimeDelta)
{
    for (auto& Pair : m_PartObjects)
    {
        if (nullptr != Pair.second)
            Pair.second->Late_Update(fTimeDelta);
    }

    if (nullptr != m_pWeaponTrail)
    {
        m_pWeaponTrail->Update(fTimeDelta);
        m_pWeaponTrail->Late_Update(fTimeDelta);
    }

    if (nullptr != m_pCollider && nullptr != m_pTransformCom)
    {
        m_pCollider->Update(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()));
        m_pCollider->Register();
    }
}

HRESULT CMonster::Render()
{
    return S_OK;
}

HRESULT CMonster::Ready_Components(const MONSTER_DESC& Desc)
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
        INVALID_INDEX == m_pNavigationAgent->Get_CurrentCellIndex())
    {
        _float3 vPosition{};
        XMStoreFloat3(&vPosition, m_pTransformCom->Get_State(STATE::POSITION));
        m_pNavigationAgent->Find_CurrentCell(vPosition);
    }

    // Body OBB Collider
    m_pCollider = CCollider::Create(m_pDevice, m_pContext);
    if (nullptr == m_pCollider)
        return E_FAIL;

    CCollider::COLLIDER_DESC ColliderDesc{};
    ColliderDesc.eBoundingType = COLLIDER::OBB;
    ColliderDesc.eGroup = COLLISION_GROUP::MONSTER_BODY;
    ColliderDesc.vCenter = _float3(0.f, 1.2f, 0.f);   // Boss 가 더 큼 → 약간 위
    ColliderDesc.vSize = _float3(1.2f, 2.4f, 1.2f); // Boss Igris 기준 임시값
    ColliderDesc.vRadians = _float3(0.f, 0.f, 0.f);
    ColliderDesc.pOwner = this;

    if (FAILED(m_pCollider->Initialize(&ColliderDesc)))
        return E_FAIL;

    //m_pCollider->Set_OnHitEnter([](CCollider* pOther) {
    //    OutputDebugStringA("[Collision] Monster Body ENTER\n");
    //    });
    //m_pCollider->Set_OnHitExit([](CCollider* pOther) {
    //    OutputDebugStringA("[Collision] Monster Body EXIT\n");
    //    });

    return S_OK;
}

HRESULT CMonster::Ready_PartObjects(const MONSTER_DESC& Desc)
{
    if (nullptr != Desc.pBodyModelPrototypeTag)
    {
        CBody_Monster::BODY_MONSTER_DESC BodyDesc{};
        BodyDesc.pParentMatrix = m_pTransformCom->Get_WorldMatrixPtr();
        BodyDesc.pModelPrototypeTag = Desc.pBodyModelPrototypeTag;
        BodyDesc.eAnimSet = Desc.eAnimSet;

        if (FAILED(__super::Add_PartObject(
            ETOUI(LEVEL::GAMEPLAY),
            TEXT("Prototype_GameObject_Body_Monster"),
            TEXT("Body"),
            &BodyDesc)))
            return E_FAIL;

        m_pBody = dynamic_cast<CBody_Monster*>(m_PartObjects[TEXT("Body")]);
        Safe_AddRef(m_pBody);
    }

    if (nullptr != Desc.pWeaponModelPrototypeTag)
    {
        if (nullptr == Desc.pWeaponSocketBoneName)
            return E_FAIL;

        const _float4x4* pSocketBoneMatrix =
            m_pBody->Get_BoneMatrixPtr(Desc.pWeaponSocketBoneName);

        if (nullptr == pSocketBoneMatrix)
            return E_FAIL;

        CWeapon::WEAPON_DESC WeaponDesc{};
        WeaponDesc.pParentMatrix        = m_pTransformCom->Get_WorldMatrixPtr();
        WeaponDesc.pSocketBoneMatrix    = pSocketBoneMatrix;
        WeaponDesc.pModelPrototypeTag   = Desc.pWeaponModelPrototypeTag;
        WeaponDesc.bInitiallyVisible    = Desc.bWeaponInitiallyVisible;
        WeaponDesc.eAttackGroup         = COLLISION_GROUP::MONSTER_ATTACK;

        if (FAILED(__super::Add_PartObject(
            ETOUI(LEVEL::GAMEPLAY),
            TEXT("Prototype_GameObject_Weapon"),
            TEXT("Igris_Weapon"),
            &WeaponDesc)))
            return E_FAIL;

        auto iter = m_PartObjects.find(TEXT("Igris_Weapon"));
        if (m_PartObjects.end() == iter)
            return E_FAIL;

        m_pWeapon = dynamic_cast<CWeapon*>(iter->second);
        if (nullptr == m_pWeapon)
            return E_FAIL;

        Safe_AddRef(m_pWeapon);

        if (nullptr != m_pWeapon->Get_BladeCollider())
        {
            m_pWeapon->Get_BladeCollider()->Set_OnHitEnter(
                [this](CCollider* pOther)
                {
                    On_WeaponHitEnter(pOther);
                });
        }
    }


    return S_OK;
}

HRESULT CMonster::Ready_WeaponTrailEffect()
{
    if (nullptr == m_pBody || nullptr == m_pWeapon)
        return S_OK;

    const _float4x4* pParentMatrix = &m_pBody->Get_CombinedWorldMatrix();
    const _float4x4* pStart = m_pBody->Get_BoneMatrixPtr("Bip001 Prop1");
    const _float4x4* pEnd = m_pBody->Get_BoneMatrixPtr("FX_Point_R_Weapon");

    if (nullptr == pStart || nullptr == pEnd)
    {
        pStart = m_pBody->Get_BoneMatrixPtr("FX_Point_Weapon");
        pEnd = m_pBody->Get_BoneMatrixPtr("FX_Point_R_Weapon");
    }

    if (nullptr == pParentMatrix || nullptr == pStart || nullptr == pEnd)
        return S_OK;

    CWeaponTrailEffect::WEAPON_TRAIL_EFFECT_DESC Desc{};
    Desc.pParentWorldMatrix = pParentMatrix;
    Desc.pStartBoneMatrix = pStart;
    Desc.pEndBoneMatrix = pEnd;
    Desc.pTexturePrototypeTag = TEXT("Prototype_Component_Texture_Effect_Trail_IgrisWeapon");
    Desc.vColor = _float4(1.35f, 0.03f, 0.02f, 1.45f);
    Desc.iMaxSamples = 36;
    Desc.fSampleInterval = 0.006f;
    Desc.fLifeTime = 0.24f;
    Desc.fMinSampleDistance = 0.03f;

    m_pWeaponTrail = dynamic_cast<CWeaponTrailEffect*>(
        m_pGameInstance->Clone_Prototype(
            PROTOTYPE::GAMEOBJECT,
            ETOUI(LEVEL::GAMEPLAY),
            TEXT("Prototype_GameObject_WeaponTrailEffect"),
            &Desc));

    return (nullptr != m_pWeaponTrail) ? S_OK : E_FAIL;
}
HRESULT CMonster::Ready_StateMachine()
{
    if (nullptr == m_pBody)
        return S_OK;

    if (MONSTER_ANIM_SET::NONE == m_eAnimSet)
        return S_OK;

    const MONSTER_ANIM_TABLE_DESC* pAnimTable = Find_MonsterAnimTable(m_eAnimSet);
    if (nullptr == pAnimTable)
        return E_FAIL;

    m_pStateMachine = CMonster_StateMachine::Create(pAnimTable);
    if (nullptr == m_pStateMachine)
        return E_FAIL;

    m_pStateMachine->Bind_Owner(this);

    m_pBody->Set_Listener(m_pStateMachine);

    if (false == m_pStateMachine->Enter_InitialState(MONSTER_ACTION::IDLE, MONSTER_ACTION_STEP::NONE))
        return E_FAIL;

    return S_OK;
}

_bool CMonster::Resolve_NavigationPosition(const _float3& vCandidatePosition, _float3* pOutPosition)
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

_bool CMonster::Try_ApplyMovementPosition(const _float3& vCandidatePosition)
{
    if (nullptr == m_pTransformCom)
        return false;

    if (nullptr == m_pNavigationAgent || false == m_pNavigationAgent->Has_NavMesh())
    {
        m_pTransformCom->Set_State(STATE::POSITION, XMVectorSetW(XMLoadFloat3(&vCandidatePosition), 1.f));
        return true;
    }

    _float3 vAdjustedPosition{};
    if (false == m_pNavigationAgent->Try_Move(vCandidatePosition, &vAdjustedPosition))
        return false;

    m_pTransformCom->Set_State(STATE::POSITION, XMVectorSetW(XMLoadFloat3(&vAdjustedPosition), 1.f));
    return true;
}

void CMonster::Apply_RootMotion(const _float3& vLocalDelta)
{
    if (0.f == vLocalDelta.x && 0.f == vLocalDelta.y && 0.f == vLocalDelta.z)
        return;

    if (nullptr == m_pTransformCom)
        return;

    _vector vRight = XMVector3Normalize(m_pTransformCom->Get_State(STATE::RIGHT));
    _vector vUp = XMVector3Normalize(m_pTransformCom->Get_State(STATE::UP));
    _vector vLook = XMVector3Normalize(m_pTransformCom->Get_State(STATE::LOOK));

    _vector vWorldDelta = XMVectorScale(vRight, vLocalDelta.x);
    vWorldDelta = XMVectorAdd(vWorldDelta, XMVectorScale(vUp, vLocalDelta.y));
    vWorldDelta = XMVectorAdd(vWorldDelta, XMVectorScale(vLook, vLocalDelta.z));

    _vector vPosition = m_pTransformCom->Get_State(STATE::POSITION);
    vPosition = XMVectorAdd(vPosition, vWorldDelta);

    _float3 vCandidatePosition{};
    XMStoreFloat3(&vCandidatePosition, vPosition);

    Try_ApplyMovementPosition(vCandidatePosition);
}

void CMonster::On_WeaponHitEnter(CCollider* pOther)
{
    if (nullptr == pOther)
        return;

    if (COLLISION_GROUP::PLAYER_BODY != pOther->Get_Group())
        return;

    CGameObject* pTarget = pOther->Get_Owner();
    if (nullptr == pTarget)
        return;

    if (m_AttackHitTargets.end() != m_AttackHitTargets.find(pTarget))
        return;

    m_AttackHitTargets.insert(pTarget);

#ifdef _DEBUG
    OutputDebugStringA("[Monster] Weapon Hit Player Body\n");
#endif

    CPlayer* pPlayer = dynamic_cast<CPlayer*>(pTarget);
    if (nullptr != pPlayer)
        pPlayer->Take_Damage(200.f, this);

    if (auto* pHUD = CHUD_GamePlay::Get_Instance())
        pHUD->Notify_Hit(this);
}

void CMonster::Tick_AI(_float fTimeDelta)
{
    if (false == Can_TickAI())
        return;

    m_fAIDecisionTimer -= fTimeDelta;
    if (m_fAIDecisionTimer > 0.f)
        return;

    m_fAIDecisionTimer = m_fAIDecisionInterval;

    CGameObject* pTarget = Resolve_Target();
    if (nullptr == pTarget)
        return;

    const _float fDistance = Compute_DistanceToTarget(pTarget);

    const MONSTER_ACTION eAction = Select_AIAction(pTarget, fDistance);

    if (MONSTER_ACTION::IDLE == eAction || MONSTER_ACTION::END == eAction)
        return;

    const MONSTER_ACTION_STEP eStep = Select_AIActionStep(eAction);

    m_pStateMachine->Try_Action(eAction, eStep);
}

void CMonster::Set_Target(CGameObject* pTarget)
{
    if (m_pTarget == pTarget)
        return;

    Safe_Release(m_pTarget);

    m_pTarget = pTarget;

    Safe_AddRef(m_pTarget);
}

CGameObject* CMonster::Resolve_Target()
{
    if (nullptr != m_pTarget)
        return m_pTarget;

    Set_Target(Find_Player());

    return m_pTarget;
}

CGameObject* CMonster::Find_Player() const
{
    if (nullptr == m_pGameInstance)
        return nullptr;

    const map<const _wstring, CLayer*>* pLayers =
        m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));

    if (nullptr == pLayers)
        return nullptr;

    auto iterLayer = pLayers->find(TEXT("Layer_Player"));
    if (pLayers->end() == iterLayer || nullptr == iterLayer->second)
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

_bool CMonster::Can_TickAI() const
{
    if (false == m_bAIEnabled)
        return false;

    if (nullptr == m_pStateMachine)
        return false;

    if (m_fCurrentHP <= 0.f)
        return false;

    if (true == m_bHasBreak && m_fCrashDurationCurrent > 0.f)
        return false;

    if (true == Is_AIActionLocked())
        return false;

    return true;
}

_bool CMonster::Is_AIActionLocked() const
{
    if (nullptr == m_pStateMachine)
        return false;

    const MONSTER_ACTION eAction = m_pStateMachine->Get_CurrentMonsterAction();

    switch (eAction)
    {
    case MONSTER_ACTION::BASIC_ATTACK_01:
    case MONSTER_ACTION::BASIC_ATTACK_02:
    case MONSTER_ACTION::BASIC_ATTACK_03:

    case MONSTER_ACTION::SKILL_01:
    case MONSTER_ACTION::SKILL_03:
    case MONSTER_ACTION::SKILL_04:
    case MONSTER_ACTION::SKILL_05:
    case MONSTER_ACTION::SKILL_06:
    case MONSTER_ACTION::SKILL_07:
    case MONSTER_ACTION::SKILL_08:
    case MONSTER_ACTION::SKILL_09:
    case MONSTER_ACTION::SKILL_10:
    case MONSTER_ACTION::SKILL_11:
    case MONSTER_ACTION::SKILL_12:
    case MONSTER_ACTION::SKILL_13:
    case MONSTER_ACTION::SKILL_14:
    case MONSTER_ACTION::SKILL_15:
    case MONSTER_ACTION::SKILL_16:

    case MONSTER_ACTION::CRASH:
    case MONSTER_ACTION::DEATH:
    case MONSTER_ACTION::INTRO:
        return true;

    default:
        return false;
    }
}

_float CMonster::Compute_DistanceToTarget(CGameObject* pTarget) const
{
    if (nullptr == pTarget)
        return FLT_MAX;

    if (nullptr == m_pTransformCom || nullptr == pTarget->Get_Transform())
        return FLT_MAX;

    _float3 vMonsterPosition = {};
    _float3 vPlayerPosition = {};

    XMStoreFloat3(&vMonsterPosition, m_pTransformCom->Get_State(STATE::POSITION));
    XMStoreFloat3(&vPlayerPosition, pTarget->Get_Transform()->Get_State(STATE::POSITION));

    const _float fX = vPlayerPosition.x - vMonsterPosition.x;
    const _float fZ = vPlayerPosition.z - vMonsterPosition.z;

    return sqrtf(fX * fX + fZ * fZ);
}

MONSTER_ACTION CMonster::Select_AIAction(CGameObject* pTarget, _float fDistance)
{
    if (nullptr == pTarget)
        return MONSTER_ACTION::IDLE;

    if (fDistance <= m_fMeleeRange)
        return MONSTER_ACTION::BASIC_ATTACK_01;

    if (fDistance <= m_fMidRange)
        return MONSTER_ACTION::SKILL_03;

    if (fDistance <= m_fLongRange)
        return MONSTER_ACTION::SKILL_05;   

    return MONSTER_ACTION::IDLE;
}

MONSTER_ACTION_STEP CMonster::Select_AIActionStep(MONSTER_ACTION eAction) const
{
    switch (eAction)
    {
    case MONSTER_ACTION::SKILL_07:
    case MONSTER_ACTION::SKILL_09:
    case MONSTER_ACTION::SKILL_10:
    case MONSTER_ACTION::SKILL_12:
    case MONSTER_ACTION::SKILL_15:
        return MONSTER_ACTION_STEP::START;

    default:
        return MONSTER_ACTION_STEP::NONE;
    }
}

void CMonster::Free()
{
    __super::Free();

    Safe_Release(m_pTarget);

    if (nullptr != m_pWeapon && nullptr != m_pWeapon->Get_BladeCollider())
        m_pWeapon->Get_BladeCollider()->Clear_Callbacks();

    m_AttackHitTargets.clear();

    if (nullptr != m_pCollider)
        m_pCollider->Clear_Callbacks();

    Safe_Release(m_pCollider);
    Safe_Release(m_pStateMachine);
    Safe_Release(m_pWeaponTrail);
    Safe_Release(m_pWeapon);
    Safe_Release(m_pBody);
    Safe_Release(m_pNavigationAgent);
}
