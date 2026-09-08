#include "Player_StateMachine.h"
#include "Player.h"
#include "HUD_GamePlay.h"

namespace
{
    CHARACTER_ACTION Get_QTEAction(QTE_TYPE eType)
    {
        switch (eType)
        {
        case QTE_TYPE::EXTREME_DASH: 
            return CHARACTER_ACTION::QTE_EXTREME_DASH;
        default:                     
            return CHARACTER_ACTION::END;
        }
    }
}

CPlayer_StateMachine::CPlayer_StateMachine()
{
}

void CPlayer_StateMachine::Get_SkillParams(_uint iStateKey, _float& fOutRadius, _float& fOutDamage, _float& fOutForwardOffset) const
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

HRESULT CPlayer_StateMachine::Initialize(const CHARACTER_ANIM_TABLE_DESC* pAnimTable)
{
    if (nullptr == pAnimTable)
        return E_FAIL;

    for (_uint i = 0; i < pAnimTable->iNumPolicies; ++i)
    {
        const CHARACTER_ACTION_POLICY& SrcPolicy = pAnimTable->pPolicies[i];

        ACTION_POLICY_BASE DstPolicy{};
        DstPolicy.iAction = Make_PlayerStateKey(SrcPolicy.eAction, SrcPolicy.eStep);
        DstPolicy.iPriority = SrcPolicy.iPriority;
        DstPolicy.bAutoReturn = SrcPolicy.bAutoReturn;
        DstPolicy.iReturnAction = Make_PlayerStateKey(SrcPolicy.eReturnAction, SrcPolicy.eReturnStep);
        DstPolicy.fCooldown = 0.f;
        DstPolicy.fEnterBlendTime = SrcPolicy.fEnterBlendTime;

        if (FAILED(Register_Policy(DstPolicy)))
            return E_FAIL;

        if (SrcPolicy.fSphereRadius > 0.f || SrcPolicy.fSphereDamage > 0.f)
        {
            SKILL_PARAMS p{};
            p.fRadius = SrcPolicy.fSphereRadius;
            p.fDamage = SrcPolicy.fSphereDamage;
            p.fForwardOffset = SrcPolicy.fSphereForwardOffset;
            m_SkillParams[DstPolicy.iAction] = p;
        }
    }

    // DASH/BACK_DASH 중 GUARD 진입 금지
    if (FAILED(Register_Reject(Make_PlayerStateKey(CHARACTER_ACTION::DASH),
        Make_PlayerStateKey(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::START))))
        return E_FAIL;
    if (FAILED(Register_Reject(Make_PlayerStateKey(CHARACTER_ACTION::BACK_DASH),
        Make_PlayerStateKey(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::START))))
        return E_FAIL;

    return S_OK;
}

void CPlayer_StateMachine::Update_LocoMotion(const PLAYER_INTENT_FRAME& Intent)
{
    const _bool bHasMoveIntent = Has_MoveIntent(Intent);
    m_bLastHasMoveIntent = bHasMoveIntent;

    if (true == Intent.bDashRequested && nullptr != m_pOwner)
    {
        if (auto* pHUD = CHUD_GamePlay::Get_Instance())
            pHUD->Notify_DashInput();

        if (true == m_pOwner->Can_ConsumeDashCharge())
        {
            const CHARACTER_ACTION eDashAction = (false == bHasMoveIntent)
                ? CHARACTER_ACTION::BACK_DASH
                : CHARACTER_ACTION::DASH;

            if (CHARACTER_ACTION::DASH == eDashAction)
                m_pOwner->Face_DirectionImmediately(Intent.vMoveDirWorld);

            if (true == Try_Action(eDashAction))
                m_pOwner->Consume_DashCharge();
            return;
        }
    }

    const CHARACTER_ACTION eCurrent = Get_CurrentCharacterAction();

    if (CHARACTER_ACTION::UNDRAW == eCurrent)
    {
        if (true == bHasMoveIntent && nullptr != m_pOwner)
        {
            m_pOwner->Set_WeaponsVisible(false);
            __super::On_ActionFinished();
        }
        return;
    }

    const _bool bIsRunAction =
        (CHARACTER_ACTION::RUN == eCurrent) ||
        (CHARACTER_ACTION::RUN_FAST == eCurrent) ||
        (CHARACTER_ACTION::RUN_FAST_LEFT == eCurrent) ||
        (CHARACTER_ACTION::RUN_FAST_RIGHT == eCurrent);

    if (bIsRunAction)
    {
        if (false == bHasMoveIntent && nullptr != m_pOwner)
        {
            const CHARACTER_ACTION eEndAction = m_pOwner->Pick_RunEndByFoot();
            Try_Action(eEndAction);
            return;
        }

        if (CHARACTER_ACTION::RUN != eCurrent && nullptr != m_pOwner)
        {
            const CHARACTER_ACTION eVariant = m_pOwner->Pick_RunFastVariant(Intent.vMoveDirWorld, eCurrent);
            if (eVariant != eCurrent)
                Try_Action(eVariant);
        }
        return;
    }

    if (CHARACTER_ACTION::RUN_END == eCurrent ||
        CHARACTER_ACTION::RUN_END_LEFT == eCurrent ||
        CHARACTER_ACTION::RUN_END_RIGHT == eCurrent)
    {
        if (true == bHasMoveIntent)
            Try_Action(CHARACTER_ACTION::RUN);
        return;
    }

    if (true == bHasMoveIntent)
        Try_Action(CHARACTER_ACTION::RUN);
    else
        Try_Action(CHARACTER_ACTION::IDLE);
}

void CPlayer_StateMachine::Update_Combat(const PLAYER_INTENT_FRAME& Intent)
{
    const CHARACTER_ACTION eCur = Get_CurrentCharacterAction();

    const _bool bIsAttacking =
        (CHARACTER_ACTION::BASIC_ATTACK_01 == eCur) ||
        (CHARACTER_ACTION::BASIC_ATTACK_02 == eCur) ||
        (CHARACTER_ACTION::BASIC_ATTACK_03 == eCur);

    const _bool bHasMoveIntent = Has_MoveIntent(Intent);

    if (true == bIsAttacking
        && true == m_bComboWindowOpen
        && true == bHasMoveIntent
        && false == Intent.bAttackRequested)
    {
        __super::On_ActionFinished();
        Try_Action(CHARACTER_ACTION::IDLE);
        m_iComboStep = 0;
        m_bComboWindowOpen = false;
        return;
    }

    if (false == Intent.bAttackRequested)
        return;

    if (auto* pHUD = CHUD_GamePlay::Get_Instance())
        pHUD->Notify_CombatInput();

    if (true == bIsAttacking)
    {
        if (false == m_bComboWindowOpen) return;

        const CHARACTER_ACTION eNext =
            (CHARACTER_ACTION::BASIC_ATTACK_01 == eCur) ? CHARACTER_ACTION::BASIC_ATTACK_02 :
            (CHARACTER_ACTION::BASIC_ATTACK_02 == eCur) ? CHARACTER_ACTION::BASIC_ATTACK_03 :
            CHARACTER_ACTION::BASIC_ATTACK_01;

        __super::On_ActionFinished();
        if (true == Try_Action(eNext))
        {
            m_iComboStep =
                (CHARACTER_ACTION::BASIC_ATTACK_02 == eNext) ? 2 :
                (CHARACTER_ACTION::BASIC_ATTACK_03 == eNext) ? 3 : 1;
            m_bComboWindowOpen = false;
        }
        return;
    }

    if (true == Try_Action(CHARACTER_ACTION::BASIC_ATTACK_01))
    {
        m_iComboStep = 1;
        m_bComboWindowOpen = false;
    }
}

void CPlayer_StateMachine::Update_Guard(const PLAYER_INTENT_FRAME& Intent)
{
    m_bLastGuardHeld = Intent.bGuardHeld;

    const CHARACTER_ACTION       eCurAction = Get_CurrentCharacterAction();
    const CHARACTER_ACTION_STEP  eCurStep = Get_CurrentCharacterStep();

    const _bool bIsGuarding =
        (CHARACTER_ACTION::GUARD == eCurAction) &&
        (CHARACTER_ACTION_STEP::START == eCurStep ||
            CHARACTER_ACTION_STEP::LOOP == eCurStep);

    // 가드 시작
    if (false == bIsGuarding && true == Intent.bGuardHeld)
    {
        if (CHARACTER_ACTION::GUARD == eCurAction
            && CHARACTER_ACTION_STEP::END == eCurStep)
            return;

        if (true == Try_Action(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::START))
        {
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }

    // 가드 종료
    if (true == bIsGuarding && false == Intent.bGuardHeld)
    {
        __super::On_ActionFinished();
        Try_Action(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::END);
        return;
    }
}

void CPlayer_StateMachine::Update_Skills(const PLAYER_INTENT_FRAME& Intent)
{
    if (nullptr == m_pOwner)
        return;

    if (true == Intent.bQTERequested && true == m_pOwner->Is_QTEWindowActive())
    {
        const QTE_TYPE eQTEType = m_pOwner->Get_LatestQTEType();   // 최근 윈도우만 확인

        // 쿨다운 중이면 발동하지 않고 무시 (윈도우는 만료까지 유지)
        if (true == m_pOwner->Is_QTEOnCooldown(eQTEType))
            return;

        const CHARACTER_ACTION eAction = Get_QTEAction(eQTEType);

        m_pOwner->Try_Teleport(12.f, 360.f);   // 재스캔(안전). 캐시 attacker 댕글링 회피
        if (true == Try_Action(eAction))
        {
            m_pOwner->Consume_LatestQTEWindow();   // 최근 1개 pop + 쿨다운 시작
#ifdef _DEBUG
            OutputDebugStringA("[QTE] Counter EXECUTED - QTE_ExtremeDash\n");
#endif
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }
    
    // C 키 — 무기 스왑
    if (true == Intent.bWeaponSwapRequested && true == m_pOwner->Can_WeaponSwap())
    {
        m_pOwner->Trigger_WeaponSwap();
        if (true == Try_Action(CHARACTER_ACTION::WEAPON_SWAP))
        {
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }

    // F 키 — 무기 고유 스킬
    if (true == Intent.bSkillFRequested && true == m_pOwner->Can_UseSkill(SKILL_SLOT::F))
    {
        const EQUIPPED_WEAPON_ID eEquipped = m_pOwner->Get_EquippedWeapon();

        if (EQUIPPED_WEAPON_ID::NONE == eEquipped)
            return;

        const CHARACTER_ACTION_STEP eStep =
            (EQUIPPED_WEAPON_ID::KNIGHT_KILLER == eEquipped)
            ? CHARACTER_ACTION_STEP::START
            : CHARACTER_ACTION_STEP::NONE;

        // R6-D: Kasaka 텔레포트 — Try_Action 직전 (타깃 없으면 무동작, 액션은 계속 진입)
        if (EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG == eEquipped)
            m_pOwner->Try_Teleport(12.f, 120.f);

        if (true == Try_Action(CHARACTER_ACTION::SKILL_F, eStep))
        {
            m_pOwner->Trigger_Skill(SKILL_SLOT::F);
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }

    // Q 키 — 스킬 Q (무기 무관 단일, Skill Collider 사용)
    if (true == Intent.bSkillQRequested && true == m_pOwner->Can_UseSkill(SKILL_SLOT::Q))
    {
        if (EQUIPPED_WEAPON_ID::NONE == m_pOwner->Get_EquippedWeapon())
            return;

        if (true == Try_Action(CHARACTER_ACTION::SKILL_Q))
        {
            m_pOwner->Trigger_Skill(SKILL_SLOT::Q);
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }

    // E 키 — 스킬 E (KnightKiller: START 진입 → 다단계 / Kasaka: NONE 단일)
    if (true == Intent.bSkillERequested && true == m_pOwner->Can_UseSkill(SKILL_SLOT::E))
    {
        const EQUIPPED_WEAPON_ID eEquipped = m_pOwner->Get_EquippedWeapon();
        if (EQUIPPED_WEAPON_ID::NONE == eEquipped)
            return;

        const CHARACTER_ACTION_STEP eStep =
            (EQUIPPED_WEAPON_ID::KNIGHT_KILLER == eEquipped)
            ? CHARACTER_ACTION_STEP::START
            : CHARACTER_ACTION_STEP::NONE;

        if (true == Try_Action(CHARACTER_ACTION::SKILL_E, eStep))
        {
            m_pOwner->Trigger_Skill(SKILL_SLOT::E);
            if (auto* pHUD = CHUD_GamePlay::Get_Instance())
                pHUD->Notify_CombatInput();
        }
        return;
    }
}

_bool CPlayer_StateMachine::Is_GuardLocked() const
{
    return (CHARACTER_ACTION::GUARD == Get_CurrentCharacterAction());
}

_bool CPlayer_StateMachine::Is_AttackLocked() const
{
    const CHARACTER_ACTION eCur = Get_CurrentCharacterAction();

    if (CHARACTER_ACTION::PARRY_COUNTER_1 == eCur ||
        CHARACTER_ACTION::PARRY_COUNTER_2 == eCur ||
        CHARACTER_ACTION::PARRY_COUNTER_3 == eCur)
        return true;

    const _bool bIsAttacking =
        (CHARACTER_ACTION::BASIC_ATTACK_01 == eCur) ||
        (CHARACTER_ACTION::BASIC_ATTACK_02 == eCur) ||
        (CHARACTER_ACTION::BASIC_ATTACK_03 == eCur);

    if (false == bIsAttacking)
        return false;

    return (false == m_bComboWindowOpen);
}

void CPlayer_StateMachine::Bind_Owner(CPlayer* pOwner)
{
    m_pOwner = pOwner;
}

_bool CPlayer_StateMachine::Enter_InitialState(CHARACTER_ACTION eInitialAction)
{
    return Try_Action(eInitialAction);
}

void CPlayer_StateMachine::OnNotify(const NOTIFY_EVENT& Event)
{
    switch (Event.eType)
    {
    case NOTIFY_TYPE::ACTION_FINISHED:
    {
        const CHARACTER_ACTION       eFinished = Get_PlayerActionFromStateKey(Event.iPayload);
        const CHARACTER_ACTION_STEP  eFinishedStep = Get_PlayerStepFromStateKey(Event.iPayload);

        if (CHARACTER_ACTION::FLOAT_END == eFinished)
        {
            break;
        }

        __super::On_ActionFinished();

        // DASH/BACK_DASH 종료
        const _bool bDashFinished =
            (CHARACTER_ACTION::DASH == eFinished) ||
            (CHARACTER_ACTION::BACK_DASH == eFinished);

        if (bDashFinished)
        {
            if (true == m_bLastHasMoveIntent)
                Try_Action(CHARACTER_ACTION::RUN_FAST);
            else
                Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        // RUN_END_* 종료
        const _bool bRunEndFinished =
            (CHARACTER_ACTION::RUN_END == eFinished) ||
            (CHARACTER_ACTION::RUN_END_LEFT == eFinished) ||
            (CHARACTER_ACTION::RUN_END_RIGHT == eFinished);

        if (bRunEndFinished)
        {
            Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        const _bool bAttackFinished =
            (CHARACTER_ACTION::BASIC_ATTACK_01 == eFinished) ||
            (CHARACTER_ACTION::BASIC_ATTACK_02 == eFinished) ||
            (CHARACTER_ACTION::BASIC_ATTACK_03 == eFinished);

        if (bAttackFinished)
        {
            m_iComboStep = 0;
            m_bComboWindowOpen = false;
            Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        // GUARD + START 종료 → LOOP 또는 END
        if (CHARACTER_ACTION::GUARD == eFinished
            && CHARACTER_ACTION_STEP::START == eFinishedStep)
        {
            if (true == m_bLastGuardHeld)
                Try_Action(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::LOOP);
            else
                Try_Action(CHARACTER_ACTION::GUARD, CHARACTER_ACTION_STEP::END);
            break;
        }
        // GUARD + END 종료 → IDLE
        else if (CHARACTER_ACTION::GUARD == eFinished
            && CHARACTER_ACTION_STEP::END == eFinishedStep)
        {
            Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        if (CHARACTER_ACTION::PARRY_COUNTER_1 == eFinished ||
            CHARACTER_ACTION::PARRY_COUNTER_2 == eFinished ||
            CHARACTER_ACTION::PARRY_COUNTER_3 == eFinished)
        {
            Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        // WEAPON_SWAP 종료 → IDLE 
        if (CHARACTER_ACTION::WEAPON_SWAP == eFinished)
        {
            Try_Action(CHARACTER_ACTION::IDLE);
            break;
        }

        // SKILL_F 종료 분기 (R6-A: 단순 자동 전이)
        if (CHARACTER_ACTION::SKILL_F == eFinished)
        {
            // Kasaka 단일 NONE 종료 → IDLE
            if (CHARACTER_ACTION_STEP::NONE == eFinishedStep)
            {
                Try_Action(CHARACTER_ACTION::IDLE);
                break;
            }
            // KnightKiller START 종료 → END (R6-A: Loop 진입 로직은 R6-B 에서 추가)
            if (CHARACTER_ACTION_STEP::START == eFinishedStep)
            {
                if (nullptr != m_pOwner)
                    m_pOwner->Enable_SkillCollider(false);
                Try_Action(CHARACTER_ACTION::SKILL_F, CHARACTER_ACTION_STEP::END);
                break;
            }
            // KnightKiller LOOP 종료 → END
            if (CHARACTER_ACTION_STEP::LOOP == eFinishedStep)
            {
                Try_Action(CHARACTER_ACTION::SKILL_F, CHARACTER_ACTION_STEP::END);
                break;
            }
            // KnightKiller END 종료 → IDLE
            if (CHARACTER_ACTION_STEP::END == eFinishedStep)
            {
                Try_Action(CHARACTER_ACTION::IDLE);
                break;
            }
        }

        // SKILL_E 종료 분기 (Kasaka NONE→IDLE / KK START→LOOP→END(End_01)→END2(End_02)→IDLE)
        if (CHARACTER_ACTION::SKILL_E == eFinished)
        {
            if (CHARACTER_ACTION_STEP::NONE == eFinishedStep)   // Kasaka 단일
            {
                Try_Action(CHARACTER_ACTION::IDLE);
                break;
            }
            if (CHARACTER_ACTION_STEP::START == eFinishedStep)
            {
                Try_Action(CHARACTER_ACTION::SKILL_E, CHARACTER_ACTION_STEP::LOOP);
                break;
            }
            if (CHARACTER_ACTION_STEP::LOOP == eFinishedStep)
            {
                Try_Action(CHARACTER_ACTION::SKILL_E, CHARACTER_ACTION_STEP::END);
                break;
            }
            if (CHARACTER_ACTION_STEP::END == eFinishedStep)    // End_01 → End_02
            {
                Try_Action(CHARACTER_ACTION::SKILL_E, CHARACTER_ACTION_STEP::END2);
                break;
            }
            if (CHARACTER_ACTION_STEP::END2 == eFinishedStep)   // End_02 → IDLE
            {
                Try_Action(CHARACTER_ACTION::IDLE);
                break;
            }
        }

        // UNDRAW 종료 → 무기 숨김
        if (CHARACTER_ACTION::UNDRAW == eFinished)
        {
            if (nullptr != m_pOwner)
                m_pOwner->Set_WeaponsVisible(false);
            break;
        }

        break;
    }
    case NOTIFY_TYPE::ANIM_EVENT:
    {
        const ANIM_NOTIFY_TYPE eAnim = static_cast<ANIM_NOTIFY_TYPE>(Event.iPayload);

        switch (eAnim)
        {
        case ANIM_NOTIFY_TYPE::FOOTSTEP_L:
        case ANIM_NOTIFY_TYPE::FOOTSTEP_R:
        {
            if (nullptr != m_pOwner)
                m_pOwner->Play_FootstepSound();
            break;
        }
        case ANIM_NOTIFY_TYPE::ATTACK_HIT:
            break;
        case ANIM_NOTIFY_TYPE::COMBO_WINDOW_OPEN:
            m_bComboWindowOpen = true;
            break;
        case ANIM_NOTIFY_TYPE::COMBO_WINDOW_CLOSE:
            m_bComboWindowOpen = false;
            break;
        case ANIM_NOTIFY_TYPE::ATTACK_HITBOX_ON:
        {
            if (nullptr != m_pOwner)
            {
                const CHARACTER_ACTION      eCur = Get_CurrentCharacterAction();
                const CHARACTER_ACTION_STEP eCurStep = Get_CurrentCharacterStep();
                const EQUIPPED_WEAPON_ID    eEq = m_pOwner->Get_EquippedWeapon();

                const _bool bKK_Loop = (CHARACTER_ACTION::SKILL_F == eCur
                    && CHARACTER_ACTION_STEP::LOOP == eCurStep
                    && EQUIPPED_WEAPON_ID::KNIGHT_KILLER == eEq);

                const _bool bQTE = (CHARACTER_ACTION::QTE_EXTREME_DASH == eCur);

                const _bool bKasaka_Slam = (CHARACTER_ACTION::SKILL_F == eCur
                    && CHARACTER_ACTION_STEP::NONE == eCurStep
                    && EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG == eEq);

                const _bool bWeaponSwap = (CHARACTER_ACTION::WEAPON_SWAP == eCur);

                const _bool bSkillQ = (CHARACTER_ACTION::SKILL_Q == eCur);
                const _bool bSkillE = (CHARACTER_ACTION::SKILL_E == eCur);

                if (true == bKK_Loop || true == bKasaka_Slam || true == bQTE || true == bWeaponSwap
                    || true == bSkillQ || true == bSkillE)
                {
                    const _uint iCur = Make_PlayerStateKey(eCur, eCurStep);

                    if (iCur != m_iLastSkillSphereStateKey)
                    {
                        m_iLastSkillSphereStateKey = iCur;
                        m_iSkillSphereOnIndex = 0;
                    }

                    _float r = 0.f, d = 0.f, o = 0.f;

                    if (true == bKasaka_Slam && m_iSkillSphereOnIndex >= 1)
                    {
                        constexpr _float KASAKA_PHASE2_RADIUS = 3.0f;
                        constexpr _float KASAKA_PHASE2_DAMAGE = 30.f;
                        constexpr _float KASAKA_PHASE2_OFFSET = 0.0f;

                        r = KASAKA_PHASE2_RADIUS;
                        d = KASAKA_PHASE2_DAMAGE;
                        o = KASAKA_PHASE2_OFFSET;
                    }
                    else
                    {
                        Get_SkillParams(iCur, r, d, o);
                    }

                    ++m_iSkillSphereOnIndex;

                    if (r > 0.f)
                    {
                        m_pOwner->Set_SkillColliderRadius(r);
                        m_pOwner->Set_SkillColliderDamage(d);
                        m_pOwner->Set_SkillColliderForwardOffset(o);
                        m_pOwner->Enable_SkillCollider(true);
                    }
                    break;   
                }
            }

            ++m_iAttackHitboxWindowSerial;
            m_bAttackHitboxActive = true;
            break;   
        }
        case ANIM_NOTIFY_TYPE::ATTACK_HITBOX_OFF:
        {
            if (nullptr != m_pOwner)
            {
                const CHARACTER_ACTION      eCur = Get_CurrentCharacterAction();
                const CHARACTER_ACTION_STEP eCurStep = Get_CurrentCharacterStep();
                const EQUIPPED_WEAPON_ID    eEq = m_pOwner->Get_EquippedWeapon();

                const _bool bKK_Loop = (CHARACTER_ACTION::SKILL_F == eCur
                    && CHARACTER_ACTION_STEP::LOOP == eCurStep
                    && EQUIPPED_WEAPON_ID::KNIGHT_KILLER == eEq);

                const _bool bKasaka_Slam = (CHARACTER_ACTION::SKILL_F == eCur
                    && CHARACTER_ACTION_STEP::NONE == eCurStep
                    && EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG == eEq);

                const _bool bQTE = (CHARACTER_ACTION::QTE_EXTREME_DASH == eCur);
                const _bool bWeaponSwap = (CHARACTER_ACTION::WEAPON_SWAP == eCur);

                const _bool bSkillQ = (CHARACTER_ACTION::SKILL_Q == eCur);
                const _bool bSkillE = (CHARACTER_ACTION::SKILL_E == eCur);

                if (true == bKK_Loop || true == bKasaka_Slam || true == bQTE || true == bWeaponSwap
                    || true == bSkillQ || true == bSkillE)
                {
                    m_pOwner->Enable_SkillCollider(false);
                    break;
                }
            }

            m_bAttackHitboxActive = false;
            break;
        }
        case ANIM_NOTIFY_TYPE::DETECT_ON:
            {
                if (nullptr == m_pOwner) 
                    break;

                const CHARACTER_ACTION      eCur = Get_CurrentCharacterAction();
                const CHARACTER_ACTION_STEP eCurStep = Get_CurrentCharacterStep();

                if (CHARACTER_ACTION::GUARD == eCur)
                {
                    m_pOwner->Set_ParryWindow(true);
#ifdef _DEBUG 
                    OutputDebugStringA("[Parry] Window ON\n");
#endif
                    break;
                }

                // SKILL_F + START + KnightKiller → skill collider ON
                if (CHARACTER_ACTION::SKILL_F == eCur
                    && CHARACTER_ACTION_STEP::START == eCurStep
                    && EQUIPPED_WEAPON_ID::KNIGHT_KILLER == m_pOwner->Get_EquippedWeapon())
                {
                    const _uint iCur = Make_PlayerStateKey(Get_CurrentCharacterAction(), Get_CurrentCharacterStep());
                    _float fRadius = 0.f, fDamage = 0.f, fOffset = 0.f;
                    Get_SkillParams(iCur, fRadius, fDamage, fOffset);
                    if (fRadius > 0.f)
                    {
                        m_pOwner->Set_SkillColliderRadius(fRadius);
                        m_pOwner->Set_SkillColliderDamage(fDamage);
                        m_pOwner->Set_SkillColliderForwardOffset(fOffset);
                        m_pOwner->Enable_SkillCollider(true);
                    }
                    break;
                }
                // 향후 다른 스킬은 여기에 분기 추가
                break;
            }
        case ANIM_NOTIFY_TYPE::DETECT_OFF:
        {
            if (nullptr == m_pOwner)
                break;

            const CHARACTER_ACTION      eCur = Get_CurrentCharacterAction();
            const CHARACTER_ACTION_STEP eCurStep = Get_CurrentCharacterStep();

            if (CHARACTER_ACTION::GUARD == eCur)
            {
                m_pOwner->Set_ParryWindow(false);
#ifdef _DEBUG
                OutputDebugStringA("[Parry] Window OFF\n");
#endif
                break;
            }

            if (CHARACTER_ACTION::SKILL_F == eCur
                && CHARACTER_ACTION_STEP::START == eCurStep
                && EQUIPPED_WEAPON_ID::KNIGHT_KILLER == m_pOwner->Get_EquippedWeapon())
            {
                m_pOwner->Enable_SkillCollider(false);
                break;
            }
            break;
        }
        case ANIM_NOTIFY_TYPE::INVINCIBLE_ON:
        {
            if (nullptr != m_pOwner)
                m_pOwner->Set_Invincible(true);
            break;
        }
        case ANIM_NOTIFY_TYPE::INVINCIBLE_OFF:
        {
            if (nullptr != m_pOwner)
                m_pOwner->Set_Invincible(false);
            break;
        }
        case ANIM_NOTIFY_TYPE::TRAIL_ON:
        {
            if (nullptr != m_pOwner)
                m_pOwner->Set_WeaponTrailActive(true);
            break;
        }
        case ANIM_NOTIFY_TYPE::TRAIL_OFF:
        {
            if (nullptr != m_pOwner)
                m_pOwner->Set_WeaponTrailActive(false);
            break;
        }

        case ANIM_NOTIFY_TYPE::NONE:
        case ANIM_NOTIFY_TYPE::END:
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

_bool CPlayer_StateMachine::Is_ReactionLocked() const
{
    const CHARACTER_ACTION eCurrent = Get_CurrentCharacterAction();

    return
        (CHARACTER_ACTION::FLOAT_A == eCurrent) ||
        (CHARACTER_ACTION::FLOAT_B == eCurrent) ||
        (CHARACTER_ACTION::FLOAT_END == eCurrent) ||
        (CHARACTER_ACTION::DOWN_RECOVERY == eCurrent) ||
        (CHARACTER_ACTION::BREAKFALL == eCurrent) ||
        (CHARACTER_ACTION::DAMAGE == eCurrent);
}

void CPlayer_StateMachine::Enter_FloatReaction(CHARACTER_ACTION eFloatAction)
{
    if (CHARACTER_ACTION::FLOAT_A != eFloatAction &&
        CHARACTER_ACTION::FLOAT_B != eFloatAction)
    {
        eFloatAction = CHARACTER_ACTION::FLOAT_A;
    }

    m_bAttackHitboxActive = false;
    m_bComboWindowOpen = false;
    m_iComboStep = 0;
    m_fDownRecoverTimer = 0.f;

    __super::On_ActionFinished();
    Try_Action(eFloatAction);
}

void CPlayer_StateMachine::Enter_ParryCounter()
{
    static const CHARACTER_ACTION s_ParryActions[] =
    {
        CHARACTER_ACTION::PARRY_COUNTER_1,
        CHARACTER_ACTION::PARRY_COUNTER_2,
        CHARACTER_ACTION::PARRY_COUNTER_3,
    };

    Try_Action(s_ParryActions[rand() % 3]);
}

void CPlayer_StateMachine::Update(_float fTimeDelta)
{
    __super::Update(fTimeDelta);

    // SKILL_E KnightKiller LOOP 차징 — 일정 시간 도달 시 END(End_01)로 자동 전환
    // (Skill_06_Loop 가 loop=true 라 ACTION_FINISHED 가 안 와서 타이머로 강제 종료)
    if (CHARACTER_ACTION::SKILL_E == Get_CurrentCharacterAction() &&
        CHARACTER_ACTION_STEP::LOOP == Get_CurrentCharacterStep())
    {
        m_fSkillELoopTimer += fTimeDelta;
        if (m_fSkillELoopTimer >= SKILL_E_LOOP_DURATION)
        {
            m_fSkillELoopTimer = 0.f;
            __super::On_ActionFinished();
            Try_Action(CHARACTER_ACTION::SKILL_E, CHARACTER_ACTION_STEP::END);
        }
        return;
    }
    m_fSkillELoopTimer = 0.f;

    if (CHARACTER_ACTION::FLOAT_END != Get_CurrentCharacterAction())
        return;

    if (m_fDownRecoverTimer > 0.f)
    {
        m_fDownRecoverTimer -= fTimeDelta;
        return;
    }

    __super::On_ActionFinished();
    Try_Action(CHARACTER_ACTION::DOWN_RECOVERY);
}

void CPlayer_StateMachine::Update_Reaction(const PLAYER_INTENT_FRAME& Intent)
{
    if (nullptr == m_pOwner)
        return;

    if (false == Intent.bDashRequested)
        return;

    const CHARACTER_ACTION eCurrent = Get_CurrentCharacterAction();

    if (CHARACTER_ACTION::FLOAT_END != eCurrent)
        return;

    if (false == m_pOwner->Can_ConsumeDashCharge())
        return;

    if (auto* pHUD = CHUD_GamePlay::Get_Instance())
        pHUD->Notify_DashInput();

    if (true == Try_Action(CHARACTER_ACTION::BREAKFALL))
    {
        m_pOwner->Consume_DashCharge();
        m_fDownRecoverTimer = 0.f;
    }
}

void CPlayer_StateMachine::On_Transition(_uint iFrom, _uint iTo, _bool bInitial)
{
    if (nullptr == m_pOwner)
        return;

    m_bAttackHitboxActive = false;

    if (nullptr != m_pOwner)
        m_pOwner->Set_WeaponTrailActive(false);

    m_pOwner->Handle_ActionTransition(
        Get_PlayerActionFromStateKey(iFrom),
        Get_PlayerStepFromStateKey(iFrom),
        Get_PlayerActionFromStateKey(iTo),
        Get_PlayerStepFromStateKey(iTo),
        bInitial);

    const CHARACTER_ACTION eTo = Get_PlayerActionFromStateKey(iTo);

    const _bool bInGuardStart =
        (CHARACTER_ACTION::GUARD == eTo &&
            CHARACTER_ACTION_STEP::START == Get_PlayerStepFromStateKey(iTo));

    if (false == bInGuardStart)
        m_pOwner->Set_ParryWindow(false);

    switch (eTo)
    {
    case CHARACTER_ACTION::WALK:
        m_pOwner->Set_SpeedCoeff(1.0f);
        break;
    case CHARACTER_ACTION::RUN:
        m_pOwner->Set_SpeedCoeff(1.5f);
        break;
    case CHARACTER_ACTION::RUN_FAST:
    case CHARACTER_ACTION::RUN_FAST_LEFT:
    case CHARACTER_ACTION::RUN_FAST_RIGHT:
        m_pOwner->Set_SpeedCoeff(2.0f);
        break;
    case CHARACTER_ACTION::IDLE:
    case CHARACTER_ACTION::RUN_END:
    case CHARACTER_ACTION::RUN_END_LEFT:
    case CHARACTER_ACTION::RUN_END_RIGHT:
    case CHARACTER_ACTION::DASH:
    case CHARACTER_ACTION::BACK_DASH:
    case CHARACTER_ACTION::BASIC_ATTACK_01:
    case CHARACTER_ACTION::BASIC_ATTACK_02:
    case CHARACTER_ACTION::BASIC_ATTACK_03:
    case CHARACTER_ACTION::GUARD:           // R2 통합
    case CHARACTER_ACTION::UNDRAW:
    case CHARACTER_ACTION::FLOAT_A:
    case CHARACTER_ACTION::FLOAT_B:
    case CHARACTER_ACTION::FLOAT_END:
    case CHARACTER_ACTION::DOWN_RECOVERY:
    case CHARACTER_ACTION::BREAKFALL:
        m_pOwner->Set_SpeedCoeff(0.f);
        break;
    default:
        m_pOwner->Set_SpeedCoeff(0.f);
        break;
    }

    switch (eTo)
    {
    case CHARACTER_ACTION::WALK:
    case CHARACTER_ACTION::RUN:
    case CHARACTER_ACTION::RUN_FAST:
    case CHARACTER_ACTION::RUN_FAST_LEFT:
    case CHARACTER_ACTION::RUN_FAST_RIGHT:
    case CHARACTER_ACTION::RUN_END:
    case CHARACTER_ACTION::RUN_END_LEFT:
    case CHARACTER_ACTION::RUN_END_RIGHT:
    case CHARACTER_ACTION::DASH:
    case CHARACTER_ACTION::BACK_DASH:
        m_pOwner->Set_WeaponsVisible(false);
        break;

    case CHARACTER_ACTION::BASIC_ATTACK_01:
    case CHARACTER_ACTION::BASIC_ATTACK_02:
    case CHARACTER_ACTION::BASIC_ATTACK_03:
    case CHARACTER_ACTION::GUARD:           // R2 통합
        m_pOwner->Set_WeaponsVisible(true);
        break;

    case CHARACTER_ACTION::PARRY_COUNTER_1:
    case CHARACTER_ACTION::PARRY_COUNTER_2:
    case CHARACTER_ACTION::PARRY_COUNTER_3:
        m_pOwner->Set_WeaponsVisible(true);
        break;

    case CHARACTER_ACTION::IDLE:
    case CHARACTER_ACTION::UNDRAW:
        break;

    case CHARACTER_ACTION::FLOAT_A:
    case CHARACTER_ACTION::FLOAT_B:
    case CHARACTER_ACTION::FLOAT_END:
    case CHARACTER_ACTION::DOWN_RECOVERY:
    case CHARACTER_ACTION::BREAKFALL:
        m_pOwner->Set_WeaponsVisible(false);
        break;

    default:
        break;
    }

    if (CHARACTER_ACTION::FLOAT_END == eTo)
        m_fDownRecoverTimer = DOWN_RECOVER_DELAY;
    if (CHARACTER_ACTION::BREAKFALL == eTo)
        m_fDownRecoverTimer = 0.f;
}

CPlayer_StateMachine* CPlayer_StateMachine::Create(const CHARACTER_ANIM_TABLE_DESC* pAnimTable)
{
    CPlayer_StateMachine* pInstance = new CPlayer_StateMachine();

    if (FAILED(pInstance->Initialize(pAnimTable)))
    {
        MSG_BOX("Failed to Created : CPlayer_StateMachine");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CPlayer_StateMachine::Free()
{
    __super::Free();
}
