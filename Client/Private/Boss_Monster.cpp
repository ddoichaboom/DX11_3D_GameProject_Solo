#include "Boss_Monster.h"
#include "GameInstance.h"
#include "Monster_StateMachine.h"
#include "NavigationAgent.h"
#include "NavMesh.h"
#include "Transform_3D.h"
#include "Collider.h"
#include "Player.h"
#include "Body_Monster.h"
#include "HUD_GamePlay.h"
#include "AreaAttackTelegraph.h"
#include "BossSlashProjectile.h"
#include "Layer.h"

CBoss_Monster::CBoss_Monster(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CMonster{ pDevice, pContext }
{
}

CBoss_Monster::CBoss_Monster(const CBoss_Monster& Prototype)
    : CMonster{ Prototype }
{
}

HRESULT CBoss_Monster::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT CBoss_Monster::Initialize(void* pArg)
{
    MONSTER_DESC Desc{};

    if (nullptr != pArg)
        Desc = *static_cast<MONSTER_DESC*>(pArg);

    Desc.eSpawnType = SPAWN_TYPE::MONSTER_BOSS;

    if (0.f == Desc.fSpeedPerSec)
        Desc.fSpeedPerSec = 4.f;

    if (0.f == Desc.fRotationPerSec)
        Desc.fRotationPerSec = XMConvertToRadians(360.f);

    if (0.f == Desc.fMaxHP)
        Desc.fMaxHP = 30000.f;
    
    if (0.f == Desc.fMaxBreak)
        Desc.fMaxBreak = 10000.f;

    Desc.bHasBreak = true;

    if (FAILED(__super::Initialize(&Desc)))
        return E_FAIL;

    m_bAIEnabled = false;
    m_fAIDecisionInterval = 0.4f;

    m_fMeleeRange = 15.0f;
    m_fMidRange = 30.0f;
    m_fLongRange = 50.0f;

    if (FAILED(Ready_SkillCollider()))
        return E_FAIL;

    return S_OK;
}

void CBoss_Monster::Begin_Encounter()
{
    if (true == m_bEncounterStarted)
        return;

    m_bEncounterStarted = true;
    m_bAIEnabled = true;
    m_fAIDecisionTimer = 0.f;

    CGameObject* pTarget = Resolve_Target();
    Face_TargetImmediately(pTarget);

    if (nullptr != m_pStateMachine)
        m_pStateMachine->Try_Action(MONSTER_ACTION::INTRO, MONSTER_ACTION_STEP::NONE);
}

void CBoss_Monster::Handle_ActionTransition(MONSTER_ACTION eFromAction, MONSTER_ACTION_STEP eFromStep,
    MONSTER_ACTION eToAction, MONSTER_ACTION_STEP eToStep, _bool bInitial)
{
    __super::Handle_ActionTransition(eFromAction, eFromStep, eToAction, eToStep, bInitial);

    if (false == bInitial)
        Play_MonsterActionSound(eToAction, eToStep);

    if (eFromAction != eToAction || eFromStep != eToStep)
        Enable_SkillCollider(false);

    if (MONSTER_ACTION::SKILL_01 == eToAction && MONSTER_ACTION_STEP::NONE == eToStep)
    {
        Begin_Skill01Dash(Resolve_Target());
        return;
    }

    if (MONSTER_ACTION::SKILL_01 == eFromAction && MONSTER_ACTION::SKILL_01 != eToAction)
        End_Skill01Dash();

    if (MONSTER_ACTION::SKILL_10 == eToAction && MONSTER_ACTION_STEP::START == eToStep)
    {
        Reset_AreaAttack();
        m_iSkillAreaCombo = 0;
    }

    if (MONSTER_ACTION::SKILL_10 == eFromAction && MONSTER_ACTION::SKILL_10 != eToAction)
    {
        Reset_AreaAttack();
        m_iSkillAreaCombo = 0;
    }

    if (MONSTER_ACTION::SKILL_11 == eToAction && MONSTER_ACTION::SKILL_11 != eFromAction)
    {
        Reset_AreaAttack();
        Begin_PendingAreaAttack(Make_CircleArea(
            12.0f,
            4.0f,
            15.f,
            m_fSkill11TelegraphDuration));
    }

    if (MONSTER_ACTION::SKILL_11 == eFromAction && MONSTER_ACTION::SKILL_11 != eToAction)
        Reset_AreaAttack();
}

void CBoss_Monster::Update(_float fTimeDelta)
{
    Tick_PatternCooldowns(fTimeDelta);

    const _bool bCrashBefore =
        nullptr != m_pStateMachine &&
        MONSTER_ACTION::CRASH == m_pStateMachine->Get_CurrentMonsterAction();

    __super::Update(fTimeDelta);

    if (nullptr == m_pStateMachine)
        return;

    const MONSTER_ACTION eAction = m_pStateMachine->Get_CurrentMonsterAction();
    const MONSTER_ACTION_STEP eStep = m_pStateMachine->Get_CurrentMonsterStep();

    const _bool bCrashNow = (MONSTER_ACTION::CRASH == eAction);
    if (true == bCrashBefore && false == bCrashNow)
        m_bPostCrashPatternPending = true;

    Tick_AreaAttack(fTimeDelta);
}

void CBoss_Monster::Late_Update(_float fTimeDelta)
{
    __super::Late_Update(fTimeDelta);

    Update_SkillCollider();
}

MONSTER_ACTION CBoss_Monster::Select_AIAction(CGameObject* pTarget, _float fDistance)
{
    if (false == m_bEncounterStarted || nullptr == pTarget)
        return MONSTER_ACTION::IDLE;

    Track_BattleRange(fDistance);

    if (true == m_bPostCrashPatternPending)
    {
        m_bPostCrashPatternPending = false;

        const MONSTER_ACTION ePostCrashAction = Select_PostCrashPattern(pTarget, fDistance);
        if (MONSTER_ACTION::END != ePostCrashAction)
            return ePostCrashAction;
    }

    if (false == m_bOpeningSkillUsed &&
        fDistance <= m_fMidRange &&
        Is_PatternReady(MONSTER_ACTION::SKILL_01))
    {
        Face_TargetImmediately(pTarget);
        m_bOpeningSkillUsed = true;
        Start_PatternCooldown(MONSTER_ACTION::SKILL_01);
        Commit_SelectedPattern(MONSTER_ACTION::SKILL_01);
        return MONSTER_ACTION::SKILL_01;
    }

    struct PATTERN_CANDIDATE
    {
        MONSTER_ACTION eAction = { MONSTER_ACTION::END };
        _float         fScore = { 0.f };
    };

    PATTERN_CANDIDATE Candidates[8]{};
    _uint iNumCandidates = 0;

    auto AddCandidate = [&](MONSTER_ACTION eAction, _float fScore)
        {
            if (false == Is_PatternReady(eAction))
                return;

            for (_uint i = 0; i < iNumCandidates; ++i)
            {
                if (Candidates[i].eAction == eAction)
                {
                    Candidates[i].fScore += fScore;
                    return;
                }
            }

            if (iNumCandidates >= _countof(Candidates))
                return;

            Candidates[iNumCandidates].eAction = eAction;
            Candidates[iNumCandidates].fScore = fScore;
            ++iNumCandidates;
        };

    if (fDistance <= m_fMeleeRange)
    {
        Face_TargetImmediately(pTarget);

        AddCandidate(MONSTER_ACTION::SKILL_04, 80.f);
        AddCandidate(MONSTER_ACTION::BASIC_ATTACK_02, 55.f);
        AddCandidate(MONSTER_ACTION::BASIC_ATTACK_01, 35.f);

        if (m_iMeleePressureCount >= 2)
            AddCandidate(MONSTER_ACTION::SKILL_13, 55.f);

        if (m_iMeleePressureCount >= 3)
            AddCandidate(MONSTER_ACTION::SKILL_11, 45.f);

        if (m_iMeleePressureCount >= 4)
            AddCandidate(MONSTER_ACTION::SKILL_10, 25.f);
    }
    else if (fDistance <= m_fMidRange)
    {
        Face_TargetImmediately(pTarget);

        AddCandidate(MONSTER_ACTION::SKILL_13, 80.f);
        AddCandidate(MONSTER_ACTION::SKILL_05, 65.f);
        AddCandidate(MONSTER_ACTION::SKILL_01, 40.f);

        if (m_iSameRangeRepeatCount >= 2)
            AddCandidate(MONSTER_ACTION::SKILL_11, 35.f);
    }
    else if (fDistance <= m_fLongRange)
    {
        Face_TargetImmediately(pTarget);

        AddCandidate(MONSTER_ACTION::SKILL_01, 75.f);
        AddCandidate(MONSTER_ACTION::SKILL_06, 70.f);
        AddCandidate(MONSTER_ACTION::SKILL_05, 45.f);

        if (m_iRangedPressureCount >= 2)
            AddCandidate(MONSTER_ACTION::SKILL_01, 25.f);
    }

    if (0 == iNumCandidates)
        return MONSTER_ACTION::IDLE;

    for (_uint i = 0; i < iNumCandidates; ++i)
    {
        MONSTER_ACTION eAction = Candidates[i].eAction;

        if (m_eLastNonIdleAction == eAction)
            Candidates[i].fScore -= 80.f;

        if (m_ePrevNonIdleAction == eAction)
            Candidates[i].fScore -= 25.f;

        if (true == m_bLastPatternWasArea && true == Is_AreaPattern(eAction))
            Candidates[i].fScore -= 60.f;

        if (true == m_bLastPatternWasProjectile && true == Is_ProjectilePattern(eAction))
            Candidates[i].fScore -= 70.f;

        if (m_iSameRangeRepeatCount >= 3 && MONSTER_ACTION::BASIC_ATTACK_01 == eAction)
            Candidates[i].fScore -= 20.f;
    }

    MONSTER_ACTION eBestAction = MONSTER_ACTION::END;
    _float fBestScore = -FLT_MAX;

    for (_uint i = 0; i < iNumCandidates; ++i)
    {
        if (Candidates[i].fScore > fBestScore)
        {
            fBestScore = Candidates[i].fScore;
            eBestAction = Candidates[i].eAction;
        }
    }

    if (MONSTER_ACTION::END == eBestAction || fBestScore <= 0.f)
        return MONSTER_ACTION::IDLE;

    Start_PatternCooldown(eBestAction);
    Commit_SelectedPattern(eBestAction);
    return eBestAction;

    return MONSTER_ACTION::IDLE;
}

MONSTER_ACTION_STEP CBoss_Monster::Select_AIActionStep(MONSTER_ACTION eAction) const
{
    if (MONSTER_ACTION::SKILL_10 == eAction)
        return MONSTER_ACTION_STEP::START;

    return __super::Select_AIActionStep(eAction);
}

void CBoss_Monster::Apply_RootMotion(const _float3& vLocalDelta)
{
    if (false == m_bSkill01DashActive ||
        nullptr == m_pStateMachine ||
        MONSTER_ACTION::SKILL_01 != m_pStateMachine->Get_CurrentMonsterAction() ||
        nullptr == m_pTransformCom)
    {
        __super::Apply_RootMotion(vLocalDelta);
        return;
    }

    _float3 vCurrentPosition{};
    XMStoreFloat3(&vCurrentPosition, m_pTransformCom->Get_State(STATE::POSITION));

    const _float fRemainX = m_vSkill01DashTargetPosition.x - vCurrentPosition.x;
    const _float fRemainZ = m_vSkill01DashTargetPosition.z - vCurrentPosition.z;
    const _float fRemainSq = fRemainX * fRemainX + fRemainZ * fRemainZ;

    _float3 vAdjustedDelta = vLocalDelta;

    if (fRemainSq <= 0.01f)
    {
        vAdjustedDelta.x = 0.f;
        vAdjustedDelta.z = 0.f;
        End_Skill01Dash();

        __super::Apply_RootMotion(vAdjustedDelta);
        return;
    }

    vAdjustedDelta.x *= m_fSkill01RootMotionScale;
    vAdjustedDelta.z *= m_fSkill01RootMotionScale;

    const _float fFrameMove = sqrtf(vAdjustedDelta.x * vAdjustedDelta.x + vAdjustedDelta.z * vAdjustedDelta.z);
    const _float fRemain = sqrtf(fRemainSq);

    if (fFrameMove > fRemain && fFrameMove > 0.f)
    {
        const _float fClampScale = fRemain / fFrameMove;
        vAdjustedDelta.x *= fClampScale;
        vAdjustedDelta.z *= fClampScale;
        End_Skill01Dash();
    }

    __super::Apply_RootMotion(vAdjustedDelta);
}

void CBoss_Monster::On_AttackHitboxNotify(_bool bActive)
{
    if (nullptr == m_pStateMachine)
    {
        __super::On_AttackHitboxNotify(bActive);
        return;
    }

    const MONSTER_ACTION eAction = m_pStateMachine->Get_CurrentMonsterAction();
    const MONSTER_ACTION_STEP eStep = m_pStateMachine->Get_CurrentMonsterStep();

    if (MONSTER_ACTION::SKILL_10 == eAction)
    {
        Handle_Skill10AreaNotify(bActive, eStep);
        return;
    }

    if (MONSTER_ACTION::SKILL_06 == eAction)
    {
        if (true == bActive)
            Spawn_Skill06SlashProjectile();

        __super::On_AttackHitboxNotify(false);
        return;
    }

    if (MONSTER_ACTION::SKILL_11 == eAction)
    {
        if (true == bActive)
            Resolve_PendingAreaAttack();
        else
            __super::On_AttackHitboxNotify(false);

        return;
    }

    _float fRadius = 0.f;
    _float fDamage = 0.f;
    _float fOffset = 0.f;

    const _uint iStateKey = Make_MonsterStateKey(eAction, eStep);
    m_pStateMachine->Get_SkillParams(iStateKey, fRadius, fDamage, fOffset);

    if (fRadius > 0.f || fDamage > 0.f)
    {
        if (true == bActive)
        {
            Set_SkillColliderRadius(fRadius);
            Set_SkillColliderDamage(fDamage);
            Set_SkillColliderForwardOffset(fOffset);
            Enable_SkillCollider(true);
        }
        else
        {
            Enable_SkillCollider(false);
        }

        __super::On_AttackHitboxNotify(false);
        return;
    }

    if (false == bActive)
    {
        __super::On_AttackHitboxNotify(false);
        return;
    }

    switch (eAction)
    {
    case MONSTER_ACTION::SKILL_04:
        Begin_AreaAttack(Make_CircleArea(6.5f, 4.0f, 10.f));
        break;

    default:
        __super::On_AttackHitboxNotify(true);
        break;
    }
}

HRESULT CBoss_Monster::Ready_SkillCollider()
{
    m_pSkillCollider = CCollider::Create(m_pDevice, m_pContext);
    if (nullptr == m_pSkillCollider)
        return E_FAIL;

    CCollider::COLLIDER_DESC Desc{};
    Desc.eBoundingType = COLLIDER::SPHERE;
    Desc.eGroup = COLLISION_GROUP::MONSTER_ATTACK;
    Desc.vCenter = _float3(0.f, 0.9f, 0.f);
    Desc.vSize = _float3(m_fSkillColliderRadius, 0.f, 0.f);
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

    return S_OK;
}

void CBoss_Monster::Enable_SkillCollider(_bool bEnable)
{
    if (true == bEnable && false == m_bSkillColliderActive)
        m_SkillHitTargets.clear();

    m_bSkillColliderActive = bEnable;
}

void CBoss_Monster::Set_SkillColliderRadius(_float fRadius)
{
    if (nullptr == m_pSkillCollider)
        return;

    m_pSkillCollider->Set_Radius(fRadius);
    m_fSkillColliderRadius = fRadius;
}

void CBoss_Monster::Update_SkillCollider()
{
    if (nullptr == m_pSkillCollider || nullptr == m_pTransformCom)
        return;

    if (false == m_bSkillColliderActive)
        return;

    _vector vLook = m_pTransformCom->Get_State(STATE::LOOK);
    vLook = XMVector3Normalize(XMVectorSetY(vLook, 0.f));

    _vector vPos = m_pTransformCom->Get_State(STATE::POSITION);
    _vector vSpherePos = XMVectorAdd(vPos, XMVectorScale(vLook, m_fSkillColliderForwardOffset));

    _matrix mWorld = XMMatrixIdentity();
    mWorld.r[3] = vSpherePos;

    m_pSkillCollider->Update(mWorld);
    m_pSkillCollider->Register();
}

void CBoss_Monster::On_SkillColliderHit(CCollider* pOther)
{
    if (false == m_bSkillColliderActive)
        return;

    if (nullptr == pOther)
        return;

    if (COLLISION_GROUP::PLAYER_BODY != pOther->Get_Group())
        return;

    CGameObject* pOwner = pOther->Get_Owner();
    if (nullptr == pOwner)
        return;

    if (m_SkillHitTargets.end() != m_SkillHitTargets.find(pOwner))
        return;

    m_SkillHitTargets.insert(pOwner);

    CPlayer* pPlayer = dynamic_cast<CPlayer*>(pOwner);
    if (nullptr == pPlayer)
        return;

    pPlayer->Take_Damage(m_fSkillColliderDamage, this);
}

void CBoss_Monster::Spawn_Skill06SlashProjectile()
{
    if (nullptr == m_pGameInstance || nullptr == m_pTransformCom)
        return;

    _vector vOwnerPos = m_pTransformCom->Get_State(STATE::POSITION);
    _vector vLook = XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f);

    CGameObject* pTarget = Resolve_Target();
    if (nullptr != pTarget && nullptr != pTarget->Get_Transform())
    {
        _vector vTargetPos = pTarget->Get_Transform()->Get_State(STATE::POSITION);
        _vector vTargetDir = XMVectorSetY(XMVectorSubtract(vTargetPos, vOwnerPos), 0.f);

        if (XMVectorGetX(XMVector3LengthSq(vTargetDir)) > 0.0001f)
            vLook = vTargetDir;
    }

    if (XMVectorGetX(XMVector3LengthSq(vLook)) <= 0.0001f)
        return;

    vLook = XMVector3Normalize(vLook);

    CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
    pTransform->Rotate_Toward_XZ(vLook, XM_PI);

    _vector vStart = vOwnerPos;
    vStart = XMVectorAdd(vStart, XMVectorScale(vLook, 2.8f));
    vStart = XMVectorAdd(vStart, XMVectorSet(0.f, 1.25f, 0.f, 0.f));

    CBossSlashProjectile::BOSS_SLASH_PROJECTILE_DESC Desc{};
    Desc.pOwnerMonster = this;
    Desc.pTexturePrototypeTag = TEXT("Prototype_Component_Texture_Effect_Slash_IgrisProjectile");
    XMStoreFloat3(&Desc.vStartPosition, vStart);
    XMStoreFloat3(&Desc.vDirection, vLook);
    Desc.vSize = _float2(5.0f, 1.8f);
    Desc.fSpeed = 38.f;
    Desc.fLifeTime = 1.25f;
    Desc.fDamage = 400.f;
    Desc.fColliderRadius = 2.4f;
    Desc.bPierce = false;

    (void)m_pGameInstance->Add_GameObject(
        ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_BossSlashProjectile"),
        ETOUI(LEVEL::GAMEPLAY), TEXT("Layer_Effect"), &Desc);
}

void CBoss_Monster::Play_MonsterActionSound(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep)
{
    if (nullptr == m_pGameInstance)
        return;

    switch (eAction)
    {
    case MONSTER_ACTION::BASIC_ATTACK_01:
    case MONSTER_ACTION::BASIC_ATTACK_02:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_WeaponSkill_1-2.wav"), SOUND_CHANNEL::MONSTER, 0.8f, false);
        break;

    case MONSTER_ACTION::SKILL_02:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_Skill_2-2_St.wav"), SOUND_CHANNEL::MONSTER, 0.85f, false);
        break;

    case MONSTER_ACTION::SKILL_04:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_Skill_4-2_St.wav"), SOUND_CHANNEL::MONSTER, 0.85f, false);
        break;

    case MONSTER_ACTION::SKILL_05:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_Skill_5-1_St.wav"), SOUND_CHANNEL::MONSTER, 0.85f, false);
        break;

    case MONSTER_ACTION::SKILL_11:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_Skill_11_St.wav"), SOUND_CHANNEL::MONSTER, 0.85f, false);
        break;

    case MONSTER_ACTION::SKILL_03:
        m_pGameInstance->Play_Sound(TEXT("Shadow_Igris_Boss_S_Skill_3-2.wav"), SOUND_CHANNEL::MONSTER, 0.85f, false);
        break;

    case MONSTER_ACTION::DEATH:
        m_pGameInstance->Play_Sound(TEXT("Igris_Boss_S_Death_1-2_St.wav"), SOUND_CHANNEL::MONSTER, 0.9f, false);
        break;

    default:
        break;
    }
}

MONSTER_ACTION CBoss_Monster::Select_PostCrashPattern(CGameObject* pTarget, _float fDistance)
{
    struct POST_CRASH_PATTERN
    {
        MONSTER_ACTION eAction = { MONSTER_ACTION::END };
        _float         fWeight = { 0.f };
        _bool          bAllowed = { false };
    };

    POST_CRASH_PATTERN Candidates[] =
    {
        { MONSTER_ACTION::SKILL_11, 35.f, fDistance <= m_fMidRange },
        { MONSTER_ACTION::SKILL_10, 25.f, fDistance <= m_fMidRange },
        { MONSTER_ACTION::SKILL_06, 25.f, fDistance > m_fMidRange && fDistance <= m_fLongRange },
        { MONSTER_ACTION::SKILL_01, 20.f, fDistance > m_fMeleeRange && fDistance <= m_fLongRange },
        { MONSTER_ACTION::SKILL_13, 20.f, fDistance <= m_fMidRange },
    };

    _float fTotalWeight = 0.f;
    for (_uint i = 0; i < _countof(Candidates); ++i)
    {
        if (true == Candidates[i].bAllowed && true == Is_PatternReady(Candidates[i].eAction))
        {
            if (m_eLastNonIdleAction == Candidates[i].eAction)
                Candidates[i].fWeight *= 0.25f;

            if (true == m_bLastPatternWasArea && true == Is_AreaPattern(Candidates[i].eAction))
                Candidates[i].fWeight *= 0.45f;

            if (true == m_bLastPatternWasProjectile && true == Is_ProjectilePattern(Candidates[i].eAction))
                Candidates[i].fWeight *= 0.35f;

            fTotalWeight += Candidates[i].fWeight;
        }
    }

    if (fTotalWeight <= 0.f)
        return MONSTER_ACTION::END;

    const _float fPick = (nullptr != m_pGameInstance)
        ? m_pGameInstance->Random(0.f, fTotalWeight)
        : 0.f;

    _float fAccumulatedWeight = 0.f;
    for (_uint i = 0; i < _countof(Candidates); ++i)
    {
        if (false == Candidates[i].bAllowed || false == Is_PatternReady(Candidates[i].eAction))
            continue;

        fAccumulatedWeight += Candidates[i].fWeight;
        if (fPick <= fAccumulatedWeight)
        {
            Face_TargetImmediately(pTarget);
            Start_PatternCooldown(Candidates[i].eAction);
            Commit_SelectedPattern(Candidates[i].eAction);
            return Candidates[i].eAction;
        }
    }

    return MONSTER_ACTION::END;
}

void CBoss_Monster::Begin_AreaAttack(const AREA_ATTACK_DESC& Desc)
{
    if (true == m_bAreaAttackPending)
        Apply_AreaAttackDamage(m_PendingAreaAttack);

    if (true == Desc.bClearHitTargetsOnBegin)
        m_AttackHitTargets.clear();

    if (Desc.fFillDuration <= 0.f)
    {
        Apply_AreaAttackDamage(Desc);
        return;
    }

    m_PendingAreaAttack = Desc;
    m_bAreaAttackPending = true;
    m_fAreaAttackElapsed = 0.f;
}

void CBoss_Monster::Tick_AreaAttack(_float fTimeDelta)
{
    if (false == m_bAreaAttackPending)
        return;

    if (m_PendingAreaAttack.fFillDuration <= 0.f)
        return;

    m_fAreaAttackElapsed += fTimeDelta;

    if (m_fAreaAttackElapsed >= m_PendingAreaAttack.fFillDuration)
    {
        Apply_AreaAttackDamage(m_PendingAreaAttack);
        Reset_AreaAttack();
    }
}

void CBoss_Monster::Apply_AreaAttackDamage(const AREA_ATTACK_DESC& Desc)
{
    CGameObject* pTarget = Resolve_Target();
    if (nullptr == pTarget)
        return;

    if (m_AttackHitTargets.end() != m_AttackHitTargets.find(pTarget))
        return;

    if (false == Is_TargetInArea(pTarget, Desc))
        return;

    m_AttackHitTargets.insert(pTarget);

    CPlayer* pPlayer = dynamic_cast<CPlayer*>(pTarget);
    if (nullptr != pPlayer)
        pPlayer->Take_Damage(Desc.fDamage, this);
}

_bool CBoss_Monster::Is_TargetInArea(CGameObject* pTarget, const AREA_ATTACK_DESC& Desc) const
{
    if (nullptr == pTarget || nullptr == pTarget->Get_Transform() || nullptr == m_pTransformCom)
        return false;

    _vector vOwnerPos = m_pTransformCom->Get_State(STATE::POSITION);
    _vector vTargetPos = pTarget->Get_Transform()->Get_State(STATE::POSITION);

    _vector vRight = XMVector3Normalize(XMVectorSetY(m_pTransformCom->Get_State(STATE::RIGHT), 0.f));
    _vector vLook = XMVector3Normalize(XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f));
    _vector vUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    _vector vCenter = vOwnerPos;
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vRight, Desc.vOffset.x));
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vUp, Desc.vOffset.y));
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vLook, Desc.vOffset.z));

    _vector vDelta = XMVectorSubtract(vTargetPos, vCenter);

    _float3 vDeltaFloat{};
    XMStoreFloat3(&vDeltaFloat, vDelta);

    switch (Desc.eShape)
    {
    case AREA_ATTACK_SHAPE::CIRCLE:
    case AREA_ATTACK_SHAPE::RING:
    {
        if (fabsf(vDeltaFloat.y) > Desc.fHeight)
            return false;

        const _float fDistanceSq = vDeltaFloat.x * vDeltaFloat.x + vDeltaFloat.z * vDeltaFloat.z;
        const _float fInnerSq = Desc.fInnerRadius * Desc.fInnerRadius;
        const _float fOuterSq = Desc.fOuterRadius * Desc.fOuterRadius;

        return fDistanceSq >= fInnerSq && fDistanceSq <= fOuterSq;
    }

    case AREA_ATTACK_SHAPE::FRONT_SPHERE:
    {
        if (fabsf(vDeltaFloat.y) > Desc.fHeight)
            return false;

        const _float fDistanceSq = XMVectorGetX(XMVector3LengthSq(vDelta));
        return fDistanceSq <= Desc.fOuterRadius * Desc.fOuterRadius;
    }

    case AREA_ATTACK_SHAPE::FRONT_BOX:
    {
        const _float fLocalX = XMVectorGetX(XMVector3Dot(vDelta, vRight));
        const _float fLocalY = XMVectorGetX(XMVector3Dot(vDelta, vUp));
        const _float fLocalZ = XMVectorGetX(XMVector3Dot(vDelta, vLook));

        return fabsf(fLocalX) <= Desc.vBoxHalfExtents.x &&
            fabsf(fLocalY) <= Desc.vBoxHalfExtents.y &&
            fabsf(fLocalZ) <= Desc.vBoxHalfExtents.z;
    }
    }

    return false;
}

void CBoss_Monster::Reset_AreaAttack()
{
    m_PendingAreaAttack = {};
    m_bAreaAttackPending = false;
    m_fAreaAttackElapsed = 0.f;
    Stop_AreaTelegraph();
}

AREA_ATTACK_DESC CBoss_Monster::Make_CircleArea(_float fRadius, _float fHeight, _float fDamage, _float fFillDuration) const
{
    AREA_ATTACK_DESC Desc{};
    Desc.eShape = AREA_ATTACK_SHAPE::CIRCLE;
    Desc.fInnerRadius = 0.f;
    Desc.fOuterRadius = fRadius;
    Desc.fHeight = fHeight;
    Desc.fDamage = fDamage;
    Desc.fFillDuration = fFillDuration;
    return Desc;
}

AREA_ATTACK_DESC CBoss_Monster::Make_RingArea(_float fInnerRadius, _float fOuterRadius, _float fHeight, _float fDamage, _float fFillDuration) const
{
    AREA_ATTACK_DESC Desc{};
    Desc.eShape = AREA_ATTACK_SHAPE::RING;
    Desc.fInnerRadius = fInnerRadius;
    Desc.fOuterRadius = fOuterRadius;
    Desc.fHeight = fHeight;
    Desc.fDamage = fDamage;
    Desc.fFillDuration = fFillDuration;
    return Desc;
}

void CBoss_Monster::Begin_PendingAreaAttack(const AREA_ATTACK_DESC& Desc)
{
    if (true == m_bAreaAttackPending)
        return;

    if (true == Desc.bClearHitTargetsOnBegin)
        m_AttackHitTargets.clear();

    m_PendingAreaAttack = Desc;
    m_PendingAreaAttack.fFillDuration = 0.f;
    m_bAreaAttackPending = true;
    m_fAreaAttackElapsed = 0.f;

    Play_AreaTelegraph(Desc);
}

void CBoss_Monster::Resolve_PendingAreaAttack()
{
    if (false == m_bAreaAttackPending)
        return;

    Apply_AreaAttackDamage(m_PendingAreaAttack);
    Reset_AreaAttack();
}

void CBoss_Monster::Handle_Skill10AreaNotify(_bool bActive, MONSTER_ACTION_STEP eStep)
{
    if (true == bActive)
    {
        if (MONSTER_ACTION_STEP::START == eStep)
        {
            m_iSkillAreaCombo = 0;

            Begin_PendingAreaAttack(Make_CircleArea(
                m_fSkill10Radius1,
                m_fSkill10Height,
                12.f));

            return;
        }

        if (MONSTER_ACTION_STEP::LOOP == eStep)
        {
            if (true == m_bAreaAttackPending)
                return;

            if (2 == m_iSkillAreaCombo)
            {
                Begin_PendingAreaAttack(Make_RingArea(
                    m_fSkill10Radius1,
                    m_fSkill10Radius2,
                    m_fSkill10Height,
                    14.f));
            }
            else if (3 == m_iSkillAreaCombo)
            {
                Begin_PendingAreaAttack(Make_RingArea(
                    m_fSkill10Radius2,
                    m_fSkill10Radius3,
                    m_fSkill10Height,
                    16.f));
            }
            else if (4 == m_iSkillAreaCombo)
            {
                Begin_PendingAreaAttack(Make_CircleArea(
                    m_fSkill10Radius3,
                    m_fSkill10Height,
                    20.f));

                if (nullptr != m_pStateMachine)
                    m_pStateMachine->Try_Action(MONSTER_ACTION::SKILL_10, MONSTER_ACTION_STEP::END);
            }

            return;
        }

        return;
    }

    if (MONSTER_ACTION_STEP::LOOP == eStep)
    {
        if (0 == m_iSkillAreaCombo)
        {
            m_iSkillAreaCombo = 1;
            Restart_Skill10Loop();
            return;
        }

        if (1 == m_iSkillAreaCombo ||
            2 == m_iSkillAreaCombo ||
            3 == m_iSkillAreaCombo)
        {
            Resolve_PendingAreaAttack();
            ++m_iSkillAreaCombo;
            Restart_Skill10Loop();
            return;
        }

        return;
    }

    if (MONSTER_ACTION_STEP::END == eStep)
    {
        if (4 == m_iSkillAreaCombo)
            Resolve_PendingAreaAttack();

        Reset_AreaAttack();
        m_iSkillAreaCombo = 0;
        return;
    }
}

void CBoss_Monster::Restart_Skill10Loop()
{
    if (nullptr == m_pBody)
        return;

    m_pBody->Play_Action(MONSTER_ACTION::SKILL_10, MONSTER_ACTION_STEP::LOOP, MONSTER_PHASE::COMMON);
}

void CBoss_Monster::Play_AreaTelegraph(const AREA_ATTACK_DESC& Desc)
{
    CAreaAttackTelegraph* pTelegraph = Find_AreaTelegraph();
    if (nullptr == pTelegraph || nullptr == m_pTransformCom)
        return;

    _vector vOwnerPos = m_pTransformCom->Get_State(STATE::POSITION);
    _vector vRight = XMVector3Normalize(XMVectorSetY(m_pTransformCom->Get_State(STATE::RIGHT), 0.f));
    _vector vLook = XMVector3Normalize(XMVectorSetY(m_pTransformCom->Get_State(STATE::LOOK), 0.f));
    _vector vUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    _vector vCenter = vOwnerPos;
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vRight, Desc.vOffset.x));
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vUp, Desc.vOffset.y));
    vCenter = XMVectorAdd(vCenter, XMVectorScale(vLook, Desc.vOffset.z));

    _float3 vCenterFloat{};
    XMStoreFloat3(&vCenterFloat, vCenter);

    const _float fDuration = (Desc.fFillDuration > 0.f) ? Desc.fFillDuration : m_fSkill10TelegraphDuration;
    pTelegraph->Play(Desc, vCenterFloat, fDuration);
}

void CBoss_Monster::Stop_AreaTelegraph()
{
    CAreaAttackTelegraph* pTelegraph = Find_AreaTelegraph();
    if (nullptr != pTelegraph)
        pTelegraph->Stop();
}

CAreaAttackTelegraph* CBoss_Monster::Find_AreaTelegraph() const
{
    if (nullptr == m_pGameInstance)
        return nullptr;

    const map<const _wstring, CLayer*>* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
    if (nullptr == pLayers)
        return nullptr;

    auto iterLayer = pLayers->find(TEXT("Layer_Effect"));
    if (pLayers->end() == iterLayer || nullptr == iterLayer->second)
        return nullptr;

    const list<CGameObject*>& Objects = iterLayer->second->Get_GameObjects();
    for (CGameObject* pObject : Objects)
    {
        if (nullptr == pObject)
            continue;

        if (pObject->Get_Name() != TEXT("AreaAttackTelegraph_Skill10"))
            continue;

        return dynamic_cast<CAreaAttackTelegraph*>(pObject);
    }

    return nullptr;
}

void CBoss_Monster::Begin_Skill01Dash(CGameObject* pTarget)
{
    End_Skill01Dash();

    if (nullptr == pTarget || nullptr == pTarget->Get_Transform() || nullptr == m_pTransformCom)
        return;

    _float3 vCurrentPosition{};
    _float3 vTargetPosition{};

    XMStoreFloat3(&vCurrentPosition, m_pTransformCom->Get_State(STATE::POSITION));
    XMStoreFloat3(&vTargetPosition, pTarget->Get_Transform()->Get_State(STATE::POSITION));

    _float fDirX = vTargetPosition.x - vCurrentPosition.x;
    _float fDirZ = vTargetPosition.z - vCurrentPosition.z;
    const _float fDistanceSq = fDirX * fDirX + fDirZ * fDirZ;

    if (fDistanceSq <= m_fSkill01StopDistance * m_fSkill01StopDistance)
        return;

    const _float fDistance = sqrtf(fDistanceSq);
    fDirX /= fDistance;
    fDirZ /= fDistance;

    Face_TargetImmediately(pTarget);

    m_vSkill01DashTargetPosition = vTargetPosition;
    m_vSkill01DashTargetPosition.x -= fDirX * m_fSkill01StopDistance;
    m_vSkill01DashTargetPosition.z -= fDirZ * m_fSkill01StopDistance;

    if (nullptr != m_pNavigationAgent && true == m_pNavigationAgent->Has_NavMesh())
    {
        CNavMesh* pNavMesh = m_pNavigationAgent->Get_NavMesh();
        const _int iTargetCellIndex = pNavMesh->Find_Cell(m_vSkill01DashTargetPosition);

        if (INVALID_INDEX != iTargetCellIndex)
            m_vSkill01DashTargetPosition.y = pNavMesh->Compute_Height(iTargetCellIndex, m_vSkill01DashTargetPosition);
        else
            m_vSkill01DashTargetPosition.y = vCurrentPosition.y;
    }
    else
    {
        m_vSkill01DashTargetPosition.y = vCurrentPosition.y;
    }

    const _float fDashDistance = max(0.f, fDistance - m_fSkill01StopDistance);
    m_fSkill01RootMotionScale = fDashDistance / m_fSkill01BaseTravelDistance;
    m_fSkill01RootMotionScale = max(0.35f, min(m_fSkill01RootMotionScale, 5.0f));
    m_bSkill01DashActive = true;
}

void CBoss_Monster::End_Skill01Dash()
{
    m_bSkill01DashActive = false;
    m_fSkill01RootMotionScale = 1.f;
    m_vSkill01DashTargetPosition = {};
}

void CBoss_Monster::Track_BattleRange(_float fDistance)
{
    _int iRangeBand = 3;

    if (fDistance <= m_fMeleeRange)
        iRangeBand = 0;
    else if (fDistance <= m_fMidRange)
        iRangeBand = 1;
    else if (fDistance <= m_fLongRange)
        iRangeBand = 2;

    if (m_iLastRangeBand == iRangeBand)
        ++m_iSameRangeRepeatCount;
    else
        m_iSameRangeRepeatCount = 0;

    m_iLastRangeBand = iRangeBand;

    if (0 == iRangeBand)
    {
        ++m_iMeleePressureCount;
        m_iRangedPressureCount = 0;
    }
    else if (2 == iRangeBand)
    {
        ++m_iRangedPressureCount;
        m_iMeleePressureCount = 0;
    }
    else if (1 == iRangeBand)
    {
        if (m_iMeleePressureCount > 0)
            --m_iMeleePressureCount;

        if (m_iRangedPressureCount > 0)
            --m_iRangedPressureCount;
    }
    else
    {
        m_iMeleePressureCount = 0;
        m_iRangedPressureCount = 0;
    }
}

void CBoss_Monster::Commit_SelectedPattern(MONSTER_ACTION eAction)
{
    if (MONSTER_ACTION::END == eAction || MONSTER_ACTION::IDLE == eAction)
        return;

    m_ePrevNonIdleAction = m_eLastNonIdleAction;
    m_eLastNonIdleAction = eAction;
    m_bLastPatternWasArea = Is_AreaPattern(eAction);
    m_bLastPatternWasProjectile = Is_ProjectilePattern(eAction);
}

_bool CBoss_Monster::Is_AreaPattern(MONSTER_ACTION eAction) const
{
    return MONSTER_ACTION::SKILL_10 == eAction ||
        MONSTER_ACTION::SKILL_11 == eAction ||
        MONSTER_ACTION::SKILL_04 == eAction;
}

_bool CBoss_Monster::Is_ProjectilePattern(MONSTER_ACTION eAction) const
{
    return MONSTER_ACTION::SKILL_06 == eAction;
}

void CBoss_Monster::Tick_PatternCooldowns(_float fTimeDelta)
{
    for (_uint i = 0; i < static_cast<_uint>(MONSTER_ACTION::END); ++i)
    {
        if (m_fPatternCooldowns[i] > 0.f)
        {
            m_fPatternCooldowns[i] -= fTimeDelta;

            if (m_fPatternCooldowns[i] < 0.f)
                m_fPatternCooldowns[i] = 0.f;
        }
    }
}

void CBoss_Monster::Start_PatternCooldown(MONSTER_ACTION eAction)
{
    const _uint iIndex = static_cast<_uint>(eAction);
    if (iIndex >= static_cast<_uint>(MONSTER_ACTION::END))
        return;

    m_fPatternCooldowns[iIndex] = Get_PatternCooldown(eAction);
}

_bool CBoss_Monster::Is_PatternReady(MONSTER_ACTION eAction) const
{
    const _uint iIndex = static_cast<_uint>(eAction);
    if (iIndex >= static_cast<_uint>(MONSTER_ACTION::END))
        return false;

    return m_fPatternCooldowns[iIndex] <= 0.f;
}

_float CBoss_Monster::Get_PatternCooldown(MONSTER_ACTION eAction) const
{
    switch (eAction)
    {
    case MONSTER_ACTION::BASIC_ATTACK_01:
        return 1.0f;

    case MONSTER_ACTION::BASIC_ATTACK_02:
        return 4.5f;

    case MONSTER_ACTION::SKILL_01:
        return 16.0f;

    case MONSTER_ACTION::SKILL_04:
        return 12.0f;

    case MONSTER_ACTION::SKILL_05:
        return 13.0f;

    case MONSTER_ACTION::SKILL_06:
        return 15.0f;

    case MONSTER_ACTION::SKILL_10:
        return 24.0f;

    case MONSTER_ACTION::SKILL_11:
        return 22.0f;

    case MONSTER_ACTION::SKILL_13:
        return 18.0f;

    default:
        return 0.f;
    }
}

_float3 CBoss_Monster::Get_DirectionToTargetXZ(CGameObject* pTarget) const
{
    _float3 vDirection{};

    if (nullptr == pTarget || nullptr == m_pTransformCom || nullptr == pTarget->Get_Transform())
        return vDirection;

    _vector vSelf = m_pTransformCom->Get_State(STATE::POSITION);
    _vector vTarget = pTarget->Get_Transform()->Get_State(STATE::POSITION);
    _vector vDir = vTarget - vSelf;
    vDir = XMVectorSetY(vDir, 0.f);

    if (XMVectorGetX(XMVector3LengthSq(vDir)) <= 0.0001f)
        return vDirection;

    XMStoreFloat3(&vDirection, XMVector3Normalize(vDir));
    return vDirection;
}

void CBoss_Monster::Face_TargetImmediately(CGameObject* pTarget)
{
    if (nullptr == m_pTransformCom)
        return;

    const _float3 vDirection = Get_DirectionToTargetXZ(pTarget);
    if (0.f == vDirection.x && 0.f == vDirection.z)
        return;

    CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
    pTransform->Rotate_Toward_XZ(XMLoadFloat3(&vDirection), XM_PI);
}

void CBoss_Monster::Face_TargetTracking(CGameObject* pTarget, _float fTimeDelta)
{
    if (nullptr == m_pTransformCom)
        return;

    const _float3 vDirection = Get_DirectionToTargetXZ(pTarget);
    if (0.f == vDirection.x && 0.f == vDirection.z)
        return;

    CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
    pTransform->Rotate_Toward_XZ(
        XMLoadFloat3(&vDirection),
        m_pTransformCom->Get_RotationPerSec() * fTimeDelta);
}

CBoss_Monster* CBoss_Monster::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CBoss_Monster* pInstance = new CBoss_Monster(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CBoss_Monster");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CGameObject* CBoss_Monster::Clone(void* pArg)
{
    CBoss_Monster* pInstance = new CBoss_Monster(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CBoss_Monster");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CBoss_Monster::Free()
{
    if (nullptr != m_pSkillCollider)
        m_pSkillCollider->Clear_Callbacks();

    Safe_Release(m_pSkillCollider);

    __super::Free();
}
