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

class CBody_Monster;
class CWeapon;
class CMonster_StateMachine;
class CPlayer;

class CLIENT_DLL CMonster abstract : public CContainerObject
{
public:
	typedef struct tagMonsterDesc : public CGameObject::GAMEOBJECT_DESC
	{
		CNavMesh*			pNavMesh = { nullptr };
		_int				iStartCellIndex = { INVALID_INDEX };

		SPAWN_TYPE			eSpawnType = { SPAWN_TYPE::END };
		MONSTER_ANIM_SET	eAnimSet = { MONSTER_ANIM_SET::NONE };
		const _tchar*		pBodyModelPrototypeTag = { nullptr };
		const _tchar*		pWeaponModelPrototypeTag = { nullptr };
		const _char*		pWeaponSocketBoneName = { nullptr };
		_bool				bWeaponInitiallyVisible = { true };

		_float				fMaxHP = { 0.f };
		_float				fMaxBreak = { 0.f };

		_bool				bHasBreak = { false };

		_int				iLevel = { 1 };
		_tchar				szDisplayName[MAX_PATH] = { };

		CGameObject*		pTarget = { nullptr };
	}MONSTER_DESC;

protected:
	CMonster(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CMonster(const CMonster& Prototype);
	virtual ~CMonster() = default;

public:
	_int						Get_Level() const { return m_iLevel; }
	const _wstring&				Get_DisplayName() const { return m_strDisplayName; }

	SPAWN_TYPE					Get_SpawnType() const { return m_eSpawnType; }
	MONSTER_ANIM_SET			Get_AnimSet() const { return m_eAnimSet; }
	_int						Get_CurrentNavCellIndex() const;

	_float						Get_MaxHP() const { return m_fMaxHP; }
	_float						Get_CurrentHP() const { return m_fCurrentHP; }

	_bool                       Has_Break() const { return m_bHasBreak; }
	_float                      Get_MaxBreak() const { return m_fMaxBreak; }
	_float                      Get_CurrentBreak() const { return m_fCurrentBreak; }

	void						Force_Break();

	void						Take_Damage(_float fAmount);

	virtual void				Handle_ActionTransition(MONSTER_ACTION eFromAction, MONSTER_ACTION_STEP eFromStep,
														MONSTER_ACTION eToAction, MONSTER_ACTION_STEP eToStep,
														_bool bInitial);

	void						Set_WeaponHitboxActive(_bool bActive);
	virtual void				On_AttackHitboxNotify(_bool bActive);

#ifdef _DEBUG
public:
	void						Debug_TryAction(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep = MONSTER_ACTION_STEP::NONE);
#endif


public:
	virtual HRESULT				Initialize_Prototype() override;
	virtual HRESULT				Initialize(void* pArg) override;
	virtual void				Priority_Update(_float fTimeDelta) override;
	virtual void				Update(_float fTimeDelta) override;
	virtual void				Late_Update(_float fTimeDelta) override;
	virtual HRESULT				Render() override;

protected:
	virtual const _tchar*		Get_DefaultName() const { return TEXT("Monster"); }
	virtual SPAWN_TYPE			Get_DefaultSpawnType() const { return SPAWN_TYPE::END; }
	virtual MONSTER_ANIM_SET	Get_DefaultAnimSet() const { return MONSTER_ANIM_SET::NONE; }
	virtual const _tchar*		Get_DefaultBodyModelPrototypeTag() const { return nullptr; }
	virtual const _tchar*		Get_DefaultWeaponModelPrototypeTag() const { return nullptr; }
	virtual const _char*		Get_DefaultWeaponSocketBoneName() const { return nullptr; }

	void						Tick_AI(_float fTimeDelta);

	void						Set_Target(CGameObject* pTarget);
	CGameObject*				Resolve_Target();
	CGameObject*				Find_Player() const;

	_bool						Can_TickAI() const;
	_bool						Is_AIActionLocked() const;

	_float						Compute_DistanceToTarget(CGameObject* pTarget) const;

	virtual MONSTER_ACTION		Select_AIAction(CGameObject* pTarget, _float fDistance);
	virtual MONSTER_ACTION_STEP	Select_AIActionStep(MONSTER_ACTION eAction) const;

protected:
	HRESULT						Ready_Components(const MONSTER_DESC& Desc);
	virtual HRESULT				Ready_PartObjects(const MONSTER_DESC& Desc);
	HRESULT						Ready_StateMachine();

	_bool                       Resolve_NavigationPosition(const _float3& vCandidatePosition, _float3* pOutPosition);
	_bool                       Try_ApplyMovementPosition(const _float3& vCandidatePosition);
	virtual void                Apply_RootMotion(const _float3& vLocalDelta);
	
	void						On_WeaponHitEnter(CCollider* pOther);

protected:
	CNavigationAgent*			m_pNavigationAgent = { nullptr };
	CBody_Monster*				m_pBody = { nullptr };
	CWeapon*					m_pWeapon = { nullptr };
	CCollider*					m_pCollider = { nullptr };
	CMonster_StateMachine*		m_pStateMachine = { nullptr };
	CGameObject*				m_pTarget = { nullptr };
	

	SPAWN_TYPE					m_eSpawnType = { SPAWN_TYPE::END };
	MONSTER_ANIM_SET			m_eAnimSet = { MONSTER_ANIM_SET::NONE };

	_float						m_fMaxHP = { 1.f };
	_float						m_fCurrentHP = { 1.f };

	_bool						m_bHasBreak = { false };
	_float						m_fMaxBreak = { 1.f };
	_float						m_fCurrentBreak = { 1.f };
	_bool						m_bPreserveBreakOnCrashExit = { false };

	_int						m_iLevel = { 1 };
	_wstring					m_strDisplayName; 

	_float						m_fCrashDurationMax = { 10.f };
	_float						m_fCrashDurationCurrent = { 0.f };

	set<CGameObject*>			m_AttackHitTargets;

	_bool						m_bAIEnabled = { true };
	_float						m_fAIDecisionInterval = { 0.5f };
	_float                      m_fAIDecisionTimer = { 0.f };

	_float                      m_fMeleeRange = { 3.5f };
	_float                      m_fMidRange = { 8.0f };
	_float                      m_fLongRange = { 14.0f };

public:
	virtual CGameObject*		Clone(void* pArg) PURE;
	virtual void				Free() override;
};

NS_END

