#pragma once

#include "Client_Defines.h"
#include "ContainerObject.h"
#include "NavMesh_Types.h"

NS_BEGIN(Engine)
class CNavMesh;
class CNavigationAgent;
class CCollider;
NS_END

NS_BEGIN(Client)

class CBody_Player;
class CWeapon;
class CWeaponTrailEffect;
class CIntentResolver;
class CPlayer_StateMachine;
class CMonster;

class CLIENT_DLL CPlayer final : public CContainerObject
{
public:
    typedef struct tagPlayerDesc : public CGameObject::GAMEOBJECT_DESC
    {
        CNavMesh* pNavMesh = { nullptr };
        _int  iStartCellIndex = { INVALID_INDEX };
    }PLAYER_DESC;

private:
    CPlayer(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CPlayer(const CPlayer& Prototype);
    virtual ~CPlayer() = default;

public:
    _float                  Get_SpeedCoeff() const { return m_fSpeedCoeff; }
    void                    Set_SpeedCoeff(_float fCoeff) { m_fSpeedCoeff = fCoeff; }

    _float                  Get_MaxHP() const { return m_fMaxHP; }
    _float                  Get_CurrentHP() const { return m_fCurrentHP; }

    _float                  Get_MaxMP() const { return m_fMaxMP; }
    _float                  Get_CurrentMP() const { return m_fCurrentMP; }

    void                    Take_Damage(_float fAmount, class CMonster* pAttacker = nullptr);
    _bool                   Try_GetDashHUDWorldPosition(_float3* pOutPosition) const;

    _bool                   Try_Teleport(_float fSearchRadius, _float fConeAngleDegrees);
    _bool                   Teleport_ToNavCell(CNavMesh* pNavMesh, _int iCellIndex);

public:
    virtual HRESULT         Initialize_Prototype() override;
    virtual HRESULT         Initialize(void* pArg) override;
    virtual void            Priority_Update(_float fTimeDelta) override;
    virtual void            Update(_float fTimeDelta) override;
    virtual void            Late_Update(_float fTimeDelta) override;
    virtual HRESULT         Render() override;

public:
    void                    Apply_RootMotion(const _float3& vLocalDelta);
    void                    Handle_ActionTransition(CHARACTER_ACTION eFromAction, CHARACTER_ACTION_STEP eFromStep,
                                                    CHARACTER_ACTION eToAction, CHARACTER_ACTION_STEP eToStep,
                                                    _bool bInitial);
    void                    Face_DirectionImmediately(const _float3& vDirWorld);
    CHARACTER_ACTION        Pick_RunEndByFoot() const;
    CHARACTER_ACTION        Pick_RunFastVariant(const _float3& vMoveDirWorld, CHARACTER_ACTION eCurrent) const;

    void                    Set_EquippedWeapon(EQUIPPED_WEAPON_ID eId);
    EQUIPPED_WEAPON_ID      Get_EquippedWeapon() const { return m_eEquippedWeapon; }
    _bool                   Can_UseWeaponSkill() const { return m_eEquippedWeapon != EQUIPPED_WEAPON_ID::NONE; }

public:
    _bool                   Can_ConsumeDashCharge() const { return m_iDashChargeCurrent > 0; }
    _bool                   Consume_DashCharge();
    _int                    Get_DashCharge() const { return m_iDashChargeCurrent; }
    _int                    Get_DashChargeMax() const { return m_iDashChargeMax; }

    void                    Set_WeaponsVisible(_bool bVisible);
    _bool                   Is_WeaponsVisible() const { return m_bWeaponsVisible; }

    void                    Tick_DashRegen(_float fTimeDelta);
    void                    Tick_WeaponHideTimer(_float fTimeDelta);

    _bool                   Can_WeaponSwap() const { return m_fWeaponSwapCooldownTimer <= 0.f; }
    void                    Trigger_WeaponSwap();

    _bool                   Can_UseSkill(SKILL_SLOT eSlot) const { return m_fSkillCooldown[ETOI(eSlot)] <= 0.f; }
    void                    Trigger_Skill(SKILL_SLOT eSlot);

    void                    Tick_SkillCooldowns(_float fTimeDelta);
    _float                  Get_WeaponSwapCooldownTimer() const { return m_fWeaponSwapCooldownTimer; }
    _float                  Get_WeaponSwapCooldownMax()   const { return WEAPON_SWAP_COOLDOWN; }
    _float                  Get_SkillCooldownTimer(SKILL_SLOT eSlot) const { return m_fSkillCooldown[ETOI(eSlot)]; }
    _float                  Get_SkillCooldownMax(SKILL_SLOT eSlot)   const { return SKILL_COOLDOWN[ETOI(eSlot)]; }

    void                    Enter_FloatReaction(CHARACTER_ACTION eFloatAction, class CMonster* pAttacker = nullptr);

    void                    Enable_SkillCollider(_bool bEnable);
    void                    Set_SkillColliderRadius(_float fRadius);
    void                    Set_SkillColliderDamage(_float fDamage) { m_fSkillColliderDamage = fDamage; }
    _float                  Get_SkillColliderDamage() const { return m_fSkillColliderDamage; }
    void                    Set_SkillColliderForwardOffset(_float fOffset) { m_fSkillColliderForwardOffset = fOffset; }
    _bool                   Is_SkillColliderActive() const { return m_bSkillColliderActive; }

    void                    Set_Invincible(_bool bInvincible);
    _bool                   Is_Invincible() const { return m_bInvincible; }

    void                    Set_ParryWindow(_bool bActive) { m_bParryWindow = bActive; }
    _bool                   Is_ParryWindow() const { return m_bParryWindow; }
    void                    Set_WeaponTrailActive(_bool bActive);
    void                    Play_FootstepSound();
    void                    On_DamageBlocked(class CMonster* pAttacker);

    _bool                   Is_QTEWindowActive() const { return false == m_QTEWindows.empty(); }
    QTE_TYPE                Get_LatestQTEType() const { return m_QTEWindows.back().eType; } 
    _bool                   Is_QTEOnCooldown(QTE_TYPE eType) const { return m_fQTECooldown[static_cast<int>(eType)] > 0.f; }
    _float                  Get_QTECooldownTimer(QTE_TYPE eType) const { return m_fQTECooldown[static_cast<int>(eType)]; }
    _float                  Get_QTECooldownMax(QTE_TYPE eType)   const { return QTE_COOLDOWN[static_cast<int>(eType)]; }
    void                    Consume_LatestQTEWindow();

private:
    _uint                   m_iState = {};
    CBody_Player*           m_pBody = { nullptr };
    CWeapon*                m_pWeaponR = { nullptr };
    CWeapon*                m_pWeaponL = { nullptr };
    CWeaponTrailEffect*     m_pWeaponTrailR = { nullptr };
    CWeaponTrailEffect*     m_pWeaponTrailL = { nullptr };
    CIntentResolver*        m_pIntentResolver = { nullptr };
    CPlayer_StateMachine*   m_pStateMachine = { nullptr };
    CCollider*              m_pCollider = { nullptr };
    CCollider*              m_pSkillCollider = { nullptr };

    set<pair<class CWeapon*, CGameObject*>>       m_AttackHitTargets;
    set<CGameObject*>       m_SkillHitTargets;

private:
    HRESULT                 Ready_PartObjects();
    HRESULT                 Ready_WeaponTrailEffects();
    HRESULT                 Ready_StateMachine();
    HRESULT                 Ready_Components(const PLAYER_DESC& Desc);

    _bool                   Resolve_NavigationPosition(const _float3& vCandidatePosition, _float3* pOutPosition);
    _bool                   Resolve_BodyBlockingPosition(const _float3& vCurrentPosition, const _float3& vCandidatePosition, _float3* pOutPosition);
    _bool                   Resolve_BodyOverlapPosition(const _float3& vPosition, _float3* pOutPosition) const;
    void                    Resolve_BodyBlockOverlap();

    _bool                   Try_ApplyMovementPosition(const _float3& vCandidatePosition);

    BODY_BLOCK_POLICY       Get_BodyBlockPolicy() const;
    _float                  Get_MonsterBodyBlockRadius(const CMonster* pMonster) const;
    void                    Add_BodyBlockCandidateCell(_int* pCandidateCells,
                                                        _uint* pNumCandidateCells,
                                                        _int iCellIndex) const;
    _bool                   Contains_BodyBlockCandidateCell(const _int* pCandidateCells,
                                                            _uint iNumCandidateCells,
                                                            _int iCellIndex) const;
    void                    Collect_BodyBlockCandidateCells(const CNavMesh* pNavMesh,
                                                                _int iCellIndex,
                                                                _int* pCandidateCells,
                                                                _uint* pNumCandidateCells) const;
    _bool                   Clip_SegmentByCircleXZ(const _float3& vCurrentPosition,
                                                    const _float3& vCandidatePosition,
                                                    const _float3& vCircleCenter,
                                                    _float fRadius,
                                                    _float* pOutT) const;

    void                    Gather_RawInput(PLAYER_RAW_INPUT_FRAME* pOutRaw);
    void                    Apply_MoveIntent(const PLAYER_INTENT_FRAME& Intent, _float fTimeDelta);

    _float                  Query_CameraYaw() const;
    void                    Apply_Loadout();

    void                    Refresh_WeaponVisibility();
    void                    Update_WeaponHitboxes();
    void                    Tick_WeaponTrailEffects(_float fTimeDelta);
    _float4                 Get_WeaponTrailColor(EQUIPPED_WEAPON_ID eWeapon) const;
    EQUIPPED_WEAPON_ID      Resolve_HandWeapon(_bool bLeftHand) const;

    void                    On_WeaponHitEnter(CWeapon* pSourceWeapon, CCollider* pOther);

    const WEAPON_INFO*      Find_WeaponInfo(EQUIPPED_WEAPON_ID eId);

    _bool                   Is_AerialAction() const;

    void                    Open_QTEWindow(class CMonster* pAttacker);
    void                    On_DodgeSucceeded(class CMonster* pAttacker);
    void                    Tick_QTEWindow(_float fTimeDelta);

    HRESULT                 Ready_SkillCollider(); 
    void                    Update_SkillCollider();
    void                    On_SkillColliderHit(CCollider* pOther);

    _bool                   Is_SkillF_KnightKiller_Start() const;
    _bool                   Is_SkillF_KnightKiller_Loop() const;

    class CMonster*         Find_Target(_float fSearchRadius, _float fConeAngleDegrees) const;
    void                    Teleport_BehindTarget(class CMonster* pTarget);

    void                    Play_PlayerActionSound(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep);


private:
    CNavigationAgent*       m_pNavigationAgent = { nullptr };

private:
    _float                  m_fIdleThreshold = { 3.f };

    _bool                   m_bWeaponsVisible = { false };
    _bool                   m_bLeftVisibleFromLoadOut = { true };

    _float                  m_fAttackBufferTimer = { 0.f };
    _float                  m_fIdleTimer = { 0.f };
    static constexpr _float ATTACK_BUFFER_DURATION = { 0.18f };

    _float                  m_fGuardHoldGraceTimer = { 0.f };
    static constexpr _float GUARD_HOLD_GRACE = { 0.10f };

    _int                    m_iDashChargeMax = { 3 };
    _int                    m_iDashChargeCurrent = { 3 };
    _float                  m_fDashRegenInterval = { 3.f }; //  ∏Æ¡® ¡÷±‚
    _float                  m_fDashRegenTimer = { 0.f };

    _float                  m_fSpeedCoeff = { 0.f };

    EQUIPPED_WEAPON_ID      m_eEquippedWeapon = { EQUIPPED_WEAPON_ID::NONE };
    _bool                   m_bPrevAttackHitboxActive = { false };
    _uint                   m_iPrevAttackHitboxWindowSerial = { 0 };

    _float                  m_fMaxHP = { 5000.f };
    _float                  m_fCurrentHP = { 5000.f };

    _float                  m_fMaxMP = { 100.f };
    _float                  m_fCurrentMP = { 100.f };

    static constexpr _uint  BODY_BLOCK_MAX_CANDIDATE_CELLS = { 16 };
    static constexpr _float WEAPON_SWAP_COOLDOWN = { 5.0f };
    static constexpr _int   SKILL_SLOT_COUNT = ETOI(SKILL_SLOT::END);
    static constexpr _float SKILL_COOLDOWN[SKILL_SLOT_COUNT] = { 5.0f, 8.0f, 10.0f, 20.0f };

    _float                  m_fWeaponSwapCooldownTimer = { 0.f };
    _float                  m_fSkillCooldown[SKILL_SLOT_COUNT] = {};

    _bool                   m_bSkillColliderActive = { false };
    _bool                   m_bInvincible = { false };
    _bool                   m_bDodgeConsumedThisInvincible = { false };

    _bool                   m_bParryWindow = { false };

    _float                  m_fSkillColliderForwardOffset = { 1.5f };
    _float                  m_fSkillColliderRadius = { 1.5f };
    _float                  m_fSkillColliderDamage = { 10.f };

    _float                  m_fSkillFStartTravelScale = { 1.6f };

    _float                  m_fKasakaPhase2Radius = { 3.0f };
    _float                  m_fKasakaPhase2Damage = { 30.f };
    _float                  m_fKasakaPhase2ForwardOffset = { 0.0f };

    struct QTE_WINDOW
    {
        QTE_TYPE            eType = QTE_TYPE::EXTREME_DASH;
        _float              fTimer = { 0.f };
        class CMonster*     pAttacker = { nullptr };
    };

    static constexpr _int   QTE_TYPE_COUNT = ETOI(QTE_TYPE::END);

    vector<QTE_WINDOW>      m_QTEWindows;
    _float                  m_fQTECooldown[QTE_TYPE_COUNT] = {};

    static constexpr _float QTE_WINDOW_DURATION = { 3.0f };
    static constexpr _float QTE_COOLDOWN[QTE_TYPE_COUNT] = { 5.0f };

public:
    static CPlayer*         Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CGameObject*    Clone(void* pArg) override;
    virtual void            Free() override;


};

NS_END