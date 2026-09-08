#pragma once
#include "Monster.h"

NS_BEGIN(Client)

class CLIENT_DLL CBoss_Monster final : public CMonster
{
private:
    CBoss_Monster(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CBoss_Monster(const CBoss_Monster& Prototype);
    virtual ~CBoss_Monster() = default;

public:
    virtual HRESULT                 Initialize_Prototype() override;
    virtual HRESULT                 Initialize(void* pArg) override;
    virtual void                    Update(_float fTimeDelta) override;
    virtual void                    Late_Update(_float fTimeDelta) override;

    void                            Begin_Encounter();
    virtual void                    Handle_ActionTransition(MONSTER_ACTION eFromAction, MONSTER_ACTION_STEP eFromStep,
                                                            MONSTER_ACTION eToAction, MONSTER_ACTION_STEP eToStep,
                                                            _bool bInitial) override;

    void                            Enable_SkillCollider(_bool bEnable);
    void                            Set_SkillColliderRadius(_float fRadius);
    void                            Set_SkillColliderDamage(_float fDamage) { m_fSkillColliderDamage = fDamage; }
    void                            Set_SkillColliderForwardOffset(_float fOffset) { m_fSkillColliderForwardOffset = fOffset; }
    _bool                           Is_SkillColliderActive() const { return m_bSkillColliderActive; }

protected:
    virtual const _tchar*           Get_DefaultName() const override { return TEXT("Boss_Monster"); }
    virtual SPAWN_TYPE              Get_DefaultSpawnType() const override { return SPAWN_TYPE::MONSTER_BOSS; }

    virtual MONSTER_ANIM_SET        Get_DefaultAnimSet() const override
    {
        return MONSTER_ANIM_SET::IGRIS_BOSS;
    }
    virtual const _tchar*           Get_DefaultBodyModelPrototypeTag() const override
    {
        return TEXT("Prototype_Component_Model_Monster_Igris_Body");
    }
    virtual const _tchar*           Get_DefaultWeaponModelPrototypeTag() const override
    {
        return TEXT("Prototype_Component_Model_Monster_Igris_Weapon");
    }
    virtual const _char*            Get_DefaultWeaponSocketBoneName() const override
    {
        return "Bip001 Prop1";
    }

protected:
    virtual MONSTER_ACTION          Select_AIAction(CGameObject* pTarget, _float fDistance) override;
    virtual MONSTER_ACTION_STEP     Select_AIActionStep(MONSTER_ACTION eAction) const override;
    virtual void                    Apply_RootMotion(const _float3& vLocalDelta) override;

    virtual void                    On_AttackHitboxNotify(_bool bActive) override;

private:
    MONSTER_ACTION                  Select_PostCrashPattern(CGameObject* pTarget, _float fDistance);

    void                            Begin_AreaAttack(const AREA_ATTACK_DESC& Desc);
    void                            Tick_AreaAttack(_float fTimeDelta);
    void                            Apply_AreaAttackDamage(const AREA_ATTACK_DESC& Desc);
    _bool                           Is_TargetInArea(CGameObject* pTarget, const AREA_ATTACK_DESC& Desc) const;
    void                            Reset_AreaAttack();

    AREA_ATTACK_DESC                Make_CircleArea(_float fRadius, _float fHeight, _float fDamage, _float fFillDuration = 0.f) const;
    AREA_ATTACK_DESC                Make_RingArea(_float fInnerRadius, _float fOuterRadius, _float fHeight, _float fDamage, _float fFillDuration = 0.f) const;

    void                            Begin_PendingAreaAttack(const AREA_ATTACK_DESC& Desc);
    void                            Resolve_PendingAreaAttack();
    void                            Handle_Skill10AreaNotify(_bool bActive, MONSTER_ACTION_STEP eStep);
    void                            Restart_Skill10Loop();
    void                            Play_AreaTelegraph(const AREA_ATTACK_DESC& Desc);
    void                            Stop_AreaTelegraph();
    class CAreaAttackTelegraph*     Find_AreaTelegraph() const;

    void                            Begin_Skill01Dash(CGameObject* pTarget);
    void                            End_Skill01Dash();

    void                            Tick_PatternCooldowns(_float fTimeDelta);
    void                            Start_PatternCooldown(MONSTER_ACTION eAction);
    _bool                           Is_PatternReady(MONSTER_ACTION eAction) const;
    _float                          Get_PatternCooldown(MONSTER_ACTION eAction) const;
    void                            Track_BattleRange(_float fDistance);
    void                            Commit_SelectedPattern(MONSTER_ACTION eAction);
    _bool                           Is_AreaPattern(MONSTER_ACTION eAction) const;
    _bool                           Is_ProjectilePattern(MONSTER_ACTION eAction) const;

    _float3                         Get_DirectionToTargetXZ(CGameObject* pTarget) const;
    void                            Face_TargetImmediately(CGameObject* pTarget);
    void                            Face_TargetTracking(CGameObject* pTarget, _float fTimeDelta);

    HRESULT                         Ready_SkillCollider();
    void                            Update_SkillCollider();
    void                            On_SkillColliderHit(CCollider* pOther);
    void                            Spawn_Skill06SlashProjectile();

    void                            Play_MonsterActionSound(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep);

private:
    _bool                           m_bEncounterStarted = { false };
    _bool                           m_bOpeningSkillUsed = { false };
    _bool                           m_bPostCrashPatternPending = { false };
    _bool                           m_bSkill01DashActive = { false };

    AREA_ATTACK_DESC                m_PendingAreaAttack = {};
    _bool                           m_bAreaAttackPending = { false };
    _float                          m_fAreaAttackElapsed = { 0.f };

    _uint                           m_iSkillAreaCombo = { 0 };

    _float                          m_fSkill10Radius1 = { 4.5f };
    _float                          m_fSkill10Radius2 = { 8.0f };
    _float                          m_fSkill10Radius3 = { 12.0f };
    _float                          m_fSkill10Height = { 4.0f };
    _float                          m_fSkill10TelegraphDuration = { 0.45f };
    _float                          m_fSkill11TelegraphDuration = { 0.65f };

    _float3                         m_vSkill01DashTargetPosition = {};
    _float                          m_fSkill01RootMotionScale = { 1.f };
    _float                          m_fSkill01StopDistance = { 2.8f };
    _float                          m_fSkill01BaseTravelDistance = { 10.5f };

    _float                          m_fPatternCooldowns[static_cast<_uint>(MONSTER_ACTION::END)] = {};
    MONSTER_ACTION                  m_eLastNonIdleAction = { MONSTER_ACTION::END };
    MONSTER_ACTION                  m_ePrevNonIdleAction = { MONSTER_ACTION::END };
    _bool                           m_bLastPatternWasArea = { false };
    _bool                           m_bLastPatternWasProjectile = { false };
    _int                            m_iLastRangeBand = { -1 };
    _uint                           m_iMeleePressureCount = { 0 };
    _uint                           m_iRangedPressureCount = { 0 };
    _uint                           m_iSameRangeRepeatCount = { 0 };

    CCollider*                      m_pSkillCollider = { nullptr };
    set<CGameObject*>               m_SkillHitTargets;
    _bool                           m_bSkillColliderActive = { false };
    _float                          m_fSkillColliderForwardOffset = { 2.5f };
    _float                          m_fSkillColliderRadius = { 2.5f };
    _float                          m_fSkillColliderDamage = { 10.f };

public:
    static CBoss_Monster*           Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CGameObject*            Clone(void* pArg) override;
    virtual void                    Free() override;
};

NS_END
