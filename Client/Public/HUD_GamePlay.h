#pragma once
#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Client)

class CMonster;
class CPlayer;
class CUI_Image;
class CUI_Text;

class CLIENT_DLL CHUD_GamePlay final : public CGameObject
{
private:
	CHUD_GamePlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CHUD_GamePlay(const CHUD_GamePlay& Prototype);
	virtual ~CHUD_GamePlay() = default;

public:
	static CHUD_GamePlay*			Get_Instance() { return s_pInstance; }

	void							Notify_Hit(CMonster* pMonster);
	void							Notify_Death(CMonster* pMonster);
	void							Notify_DashInput();
	void							Notify_CombatInput();

public:
	virtual HRESULT					Initialize_Prototype() override;
	virtual HRESULT					Initialize(void* pArg) override;
	virtual void					Priority_Update(_float fTimeDelta) override;
	virtual void					Update(_float fTimeDelta) override;
	virtual void					Late_Update(_float fTimeDelta) override;
	virtual HRESULT					Render() override;

private:
	void							Cache_UIs();
	void							Resolve_Player();
	void                            Cache_Dash_Offsets();
	void                            Cache_Viewport();
	CUI_Image*						Find_UI_ByName(const _tchar* pName);

	void                            Tick_LazyBar(_float& fFill, _float& fReduce, _float& fDelay,
													_float fTarget, _float fTimeDelta);
	void							Tick_Sweep(_float fTimeDelta);
	void                            Apply_Bar_Visuals(HUD_SLOT eFill, HUD_SLOT eReduce,
													_float fFill, _float fReduce);

	void                            Tick_MonsterBars(_float fTimeDelta);
	void                            Tick_PlayerBars(_float fTimeDelta);
	void                            Tick_Dash(_float fTimeDelta);
	void                            Tick_Skills(_float fTimeDelta);
	void                            Refresh_SkillIcons(EQUIPPED_WEAPON_ID eWeapon);

	void                            Set_MonsterBars_Visible(_bool bVisible);
	void							Set_PlayerBars_Visible(_bool bVisible);
	void							Set_PlayerDash_Visible(_bool bVisible);

private:
	static CHUD_GamePlay*			s_pInstance;
	
	CMonster*						m_pCurrentTarget = { nullptr };
	_float							m_fSinceLastHit = { 0.f };

	CPlayer*						m_pPlayer = { nullptr };
	EQUIPPED_WEAPON_ID              m_eCachedWeaponId = { EQUIPPED_WEAPON_ID::NONE };
	CUI_Text*                       m_pSkillKeyText[6] = {};
	_float                          m_fSkillActiveFlash[5] = {};
	_float                          m_fPrevSkillCooldown[5] = {};
	CUI_Text*                       m_pQuestText[3] = {};
	static constexpr _float         SKILL_ACTIVE_FLASH = { 0.6f };
	CUI_Image*						m_pUI[ETOUI(HUD_SLOT::END)] = {};
	CUI_Text*						m_pUI_MonsterLevel = { nullptr };
	CUI_Text*						m_pUI_MonsterName = { nullptr };
	CMonster*						m_pLastTextTarget = { nullptr };

	_bool							m_bCached = { false };

	// Reduce ¿ë Lazy Bar 
	_float                          m_fMonHpFill = { 1.f };
	_float                          m_fMonHpReduce = { 1.f };
	_float                          m_fMonHpReduceDelay = { 0.f };

	_float                          m_fMonBreakFill = { 1.f };
	_float                          m_fMonBreakReduce = { 1.f };
	_float                          m_fMonBreakReduceDelay = { 0.f };

	_float                          m_fPlyHpFill = { 1.f };
	_float                          m_fPlyHpReduce = { 1.f };
	_float                          m_fPlyHpReduceDelay = { 0.f };

	_float                          m_fPlyMpFill = { 1.f };
	_float                          m_fPlyMpReduce = { 1.f };
	_float                          m_fPlyMpReduceDelay = { 0.f };

	// Dash
	_bool                           m_bDashBaseCached = { false };
	_float                          m_fDashOffsetX[ETOUI(HUD_SLOT::END)] = {};
	_float                          m_fDashOffsetY[ETOUI(HUD_SLOT::END)] = {};
	_float                          m_fViewW = { 1280.f };
	_float                          m_fViewH = { 720.f };

	_bool							m_bDashInput = { false };
	_float							m_fSinceDashInput = { 0.f };

	_bool							m_bCombatInput = { false };
	_float							m_fSinceCombatInput = { 0.f };

	_bool							m_bGlowTrigger[3] = { false, false, false, };

	_float							m_fBarSweepTime = { 0.f };
	_float							m_fGlowSweepTime = { 0.f };

	static constexpr _float         REDUCE_HOLD = { 1.0f };
	static constexpr _float         REDUCE_LERP_SPEED = { 0.6f };
	
	static constexpr _float			DASH_VISIBLE_FOR = { 4.0f };
	static constexpr _float			BARS_VISIBLE_FOR = { 5.0f };

	static constexpr _float			BAR_SWEEP_DURATION = { 1.5f };
	static constexpr _float			BAR_SWEEP_REST = { 1.0f };
	static constexpr _float			BAR_SWEEP_TOTAL = { BAR_SWEEP_DURATION + BAR_SWEEP_REST };

	static constexpr _float			GLOW_UV_SPEED = { 0.8f };

public:
	static CHUD_GamePlay*			Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject*			Clone(void* pArg) override;
	virtual void					Free() override;
};

NS_END