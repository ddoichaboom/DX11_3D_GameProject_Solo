#include "Monster_StateMachine.h"
#include "Monster.h"

CMonster_StateMachine::CMonster_StateMachine()
{
}

MONSTER_ACTION CMonster_StateMachine::Get_CurrentMonsterAction() const
{
    if (false == Has_CurrentAction())
        return MONSTER_ACTION::END;

    return Get_MonsterActionFromStateKey(Get_CurrentAction());
}

MONSTER_ACTION_STEP  CMonster_StateMachine::Get_CurrentMonsterStep() const
{
    if (false == Has_CurrentAction())
        return MONSTER_ACTION_STEP::END;

    return Get_MonsterStepFromStateKey(Get_CurrentAction());
}

void CMonster_StateMachine::Get_SkillParams(_uint iStateKey, _float& fOutRadius, _float& fOutDamage, _float& fOutForwardOffset) const
{
    auto it = m_SkillParams.find(iStateKey);
    if (m_SkillParams.end() == it)
    {
        fOutRadius = 0.f;
        fOutDamage = 0.f;
        fOutForwardOffset = 0.f;
        return;
    }

    fOutRadius = it->second.fRadius;
    fOutDamage = it->second.fDamage;
    fOutForwardOffset = it->second.fForwardOffset;
}

HRESULT CMonster_StateMachine::Initialize(const MONSTER_ANIM_TABLE_DESC* pAnimTable)
{
    if (nullptr == pAnimTable)
        return E_FAIL;

    for (_uint i = 0; i < pAnimTable->iNumPolicies; ++i)
    {
        const MONSTER_ACTION_POLICY& SrcPolicy = pAnimTable->pPolicies[i];

        ACTION_POLICY_BASE DstPolicy{};
        DstPolicy.iAction = Make_MonsterStateKey(SrcPolicy.eAction, SrcPolicy.eStep);
        DstPolicy.iPriority = SrcPolicy.iPriority;
        DstPolicy.bAutoReturn = SrcPolicy.bAutoReturn;
        DstPolicy.iReturnAction = Make_MonsterStateKey(SrcPolicy.eReturnAction, SrcPolicy.eReturnStep);
        DstPolicy.fCooldown = SrcPolicy.fCooldown;
        DstPolicy.fEnterBlendTime = SrcPolicy.fEnterBlendTime;

        if (FAILED(Register_Policy(DstPolicy)))
            return E_FAIL;

        if (SrcPolicy.fSphereRadius > 0.f || SrcPolicy.fSphereDamage > 0.f)
        {
            SKILL_PARAMS Params{};
            Params.fRadius = SrcPolicy.fSphereRadius;
            Params.fDamage = SrcPolicy.fSphereDamage;
            Params.fForwardOffset = SrcPolicy.fSphereForwardOffset;

            m_SkillParams[DstPolicy.iAction] = Params;
        }
    }

    return S_OK;
}

void CMonster_StateMachine::Bind_Owner(CMonster* pOwner)
{
    m_pOwner = pOwner;
}

_bool CMonster_StateMachine::Enter_InitialState(MONSTER_ACTION eInitialAction, MONSTER_ACTION_STEP eInitialStep)
{
    return Try_Action(eInitialAction, eInitialStep);
}

_bool CMonster_StateMachine::Try_Action(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep)
{
    return Try_Transition(Make_MonsterStateKey(eAction, eStep));
}

void CMonster_StateMachine::OnNotify(const NOTIFY_EVENT& Event)
{
    switch (Event.eType)
    {
    case NOTIFY_TYPE::ACTION_FINISHED:
    {
        if (Has_CurrentAction() && Event.iPayload != Get_CurrentAction())
            return;

        __super::On_ActionFinished();
        break;
    }

    case NOTIFY_TYPE::ANIM_EVENT:
    {
        const ANIM_NOTIFY_TYPE eAnimNotify = static_cast<ANIM_NOTIFY_TYPE>(Event.iPayload);

        switch (eAnimNotify)
        {
        case ANIM_NOTIFY_TYPE::ATTACK_HITBOX_ON:
            if (nullptr != m_pOwner)
                m_pOwner->On_AttackHitboxNotify(true);
            break;

        case ANIM_NOTIFY_TYPE::ATTACK_HITBOX_OFF:
            if (nullptr != m_pOwner)
                m_pOwner->On_AttackHitboxNotify(false);
            break;

        case ANIM_NOTIFY_TYPE::TRAIL_ON:
            if (nullptr != m_pOwner)
                m_pOwner->Set_WeaponTrailActive(true);
            break;

        case ANIM_NOTIFY_TYPE::TRAIL_OFF:
            if (nullptr != m_pOwner)
                m_pOwner->Set_WeaponTrailActive(false);
            break;
        }
    }
        break;

    default:
        break;
    }
}

void CMonster_StateMachine::On_Transition(_uint iFrom, _uint iTo, _bool bInitial)
{
    if (nullptr == m_pOwner)
        return;

    m_pOwner->Handle_ActionTransition(
        Get_MonsterActionFromStateKey(iFrom),
        Get_MonsterStepFromStateKey(iFrom),
        Get_MonsterActionFromStateKey(iTo),
        Get_MonsterStepFromStateKey(iTo),
        bInitial);
}

CMonster_StateMachine* CMonster_StateMachine::Create(const MONSTER_ANIM_TABLE_DESC* pAnimTable)
{
    CMonster_StateMachine* pInstance = new CMonster_StateMachine();

    if (FAILED(pInstance->Initialize(pAnimTable)))
    {
        MSG_BOX("Failed to Created : CMonster_StateMachine");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CMonster_StateMachine::Free()
{
    __super::Free();
}
