#pragma once

#include "Client_Defines.h"
#include "StateMachine.h"
#include "NotifyListener.h"

NS_BEGIN(Client)

class CMonster;

class CLIENT_DLL  CMonster_StateMachine final : public CStateMachine, public INotifyListener
{
private:
	CMonster_StateMachine();
	virtual ~CMonster_StateMachine() = default;

public:
	MONSTER_ACTION					Get_CurrentMonsterAction() const;
	MONSTER_ACTION_STEP				Get_CurrentMonsterStep() const;
	void							Get_SkillParams(_uint iStateKey, _float& fOutRadius, _float& fOutDamage, _float& fOutForwardOffset) const;
public:
	HRESULT							Initialize(const MONSTER_ANIM_TABLE_DESC* pAnimTable);

	void							Bind_Owner(CMonster* pOwner);
	_bool							Enter_InitialState(MONSTER_ACTION eInitialAction = MONSTER_ACTION::IDLE,
														MONSTER_ACTION_STEP eInitialStep = MONSTER_ACTION_STEP::NONE);
	_bool							Try_Action(MONSTER_ACTION eAction,
												MONSTER_ACTION_STEP eStep = MONSTER_ACTION_STEP::NONE);

	virtual	void					OnNotify(const NOTIFY_EVENT& Event) override;

protected:
	virtual void					On_Transition(_uint iFrom, _uint iTo, _bool bInitial) override;

private:
	struct SKILL_PARAMS
	{
		_float fRadius = { 0.f };
		_float fDamage = { 0.f };
		_float fForwardOffset = { 0.f };
	};

	CMonster*						m_pOwner = { nullptr };
	map<_uint, SKILL_PARAMS>		m_SkillParams;

public:
	static CMonster_StateMachine*	Create(const MONSTER_ANIM_TABLE_DESC* pAnimTable);
	virtual void					Free() override;

};

NS_END

