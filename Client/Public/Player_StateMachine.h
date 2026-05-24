#pragma once

#include "Client_Defines.h"
#include "StateMachine.h"
#include "NotifyListener.h"

NS_BEGIN(Client)
class CPlayer;

class CLIENT_DLL CPlayer_StateMachine final : public CStateMachine, public INotifyListener
{
private:
	CPlayer_StateMachine();
	virtual ~CPlayer_StateMachine() = default;

public:
	CHARACTER_ACTION				Get_CurrentCharacterAction() const
	{
		if (false == Has_CurrentAction())
			return CHARACTER_ACTION::END;
		return Get_PlayerActionFromStateKey(Get_CurrentAction());
	}
	CHARACTER_ACTION_STEP			Get_CurrentCharacterStep() const 
	{
		if (false == Has_CurrentAction())
			return CHARACTER_ACTION_STEP::NONE;

		return Get_PlayerStepFromStateKey(Get_CurrentAction());
	}
	_bool                           Try_Action(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep = CHARACTER_ACTION_STEP::NONE)
	{
		return Try_Transition(Make_PlayerStateKey(eAction, eStep));
	}

	_bool							Try_Action_External(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep = CHARACTER_ACTION_STEP::NONE)
	{
		return Try_Action(eAction, eStep);
	}

	_bool							Is_AttackHitboxActive() const { return m_bAttackHitboxActive; }
	_uint							Get_AttackHitboxWindowSerial() const { return m_iAttackHitboxWindowSerial; }

	void							Get_SkillParams(_uint iStateKey, _float& fOutRadius, _float& fOutDamage, _float& fOutForwardOffset) const;

public:
	HRESULT							Initialize(const CHARACTER_ANIM_TABLE_DESC* pAnimTable);

public:
	void							Update_LocoMotion(const PLAYER_INTENT_FRAME& Intent);
	void							Update_Combat(const PLAYER_INTENT_FRAME& Intent);
	void							Update_Guard(const PLAYER_INTENT_FRAME& Intent);
	void							Update_Skills(const PLAYER_INTENT_FRAME& Intent);

	_bool							Is_GuardLocked() const;
	_bool							Is_AttackLocked() const;
	void							Bind_Owner(CPlayer* pOwner);
	_bool							Enter_InitialState(CHARACTER_ACTION eInitialAction = CHARACTER_ACTION::IDLE);
	virtual void					OnNotify(const NOTIFY_EVENT& Event) override;

	_bool                           Is_ReactionLocked() const;
	void                            Enter_FloatReaction(CHARACTER_ACTION eFloatAction);
	void							Enter_ParryCounter();
	void                            Update(_float fTimeDelta);
	void                            Update_Reaction(const PLAYER_INTENT_FRAME& Intent);
protected:
	virtual void					On_Transition(_uint iFrom, _uint iTo, _bool bInitial) override;

private:
	CPlayer*						m_pOwner = { nullptr };

private:
	struct SKILL_PARAMS { 
		_float						fRadius = {};
		_float						fDamage = {};
		_float						fForwardOffset = {};
	};

	_bool							m_bLastHasMoveIntent = { false };
	_bool							m_bComboWindowOpen = { false };
	_int							m_iComboStep = { 0 };
	_bool							m_bLastGuardHeld = { false };
	_bool							m_bAttackHitboxActive = { false };
	_uint                           m_iAttackHitboxWindowSerial = { 0 };
	_float							m_fDownRecoverTimer = { 0.f };
	static constexpr _float			DOWN_RECOVER_DELAY = { 0.5f };

	_float							m_fSkillELoopTimer = { 0.f };
	static constexpr _float			SKILL_E_LOOP_DURATION = { 1.0f };

	unordered_map<_uint, SKILL_PARAMS>  m_SkillParams;

	_uint							m_iSkillSphereOnIndex = { 0 };
	_uint							m_iLastSkillSphereStateKey = { 0 };



public:
	static CPlayer_StateMachine*	Create(const CHARACTER_ANIM_TABLE_DESC* pAnimTable);
	virtual void					Free() override;
};

NS_END