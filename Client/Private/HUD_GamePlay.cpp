#include "HUD_GamePlay.h"
#include "GameInstance.h"
#include "Layer.h"
#include "UI_Image.h"
#include "UIObject.h"
#include "Monster.h"
#include "Player.h"
#include "Transform.h"
#include "UI_Text.h"
#include "UI_SpriteAnim.h"
#include "AtlasInstanceEffect.h"

CHUD_GamePlay* CHUD_GamePlay::s_pInstance = { nullptr };

CHUD_GamePlay::CHUD_GamePlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CHUD_GamePlay::CHUD_GamePlay(const CHUD_GamePlay& Prototype)
	: CGameObject{ Prototype }
{
}

void CHUD_GamePlay::Notify_Hit(CMonster* pMonster)
{
	if (nullptr == pMonster)
		return;

	const _bool bTargetChanged = (m_pCurrentTarget != pMonster);

	if (bTargetChanged)
	{
		Safe_Release(m_pCurrentTarget);
		m_pCurrentTarget = pMonster;
		Safe_AddRef(m_pCurrentTarget);

		const _float fHpMax = m_pCurrentTarget->Get_MaxHP();
		const _float fHpCur = m_pCurrentTarget->Get_CurrentHP();
		const _float fHpR = (fHpMax > 0.f) ? (fHpCur / fHpMax) : 0.f;
		m_fMonHpFill = m_fMonHpReduce = fHpR;
		m_fMonHpReduceDelay = 0.f;

		if (m_pCurrentTarget->Has_Break())
		{
			const _float fBrMax = m_pCurrentTarget->Get_MaxBreak();
			const _float fBrCur = m_pCurrentTarget->Get_CurrentBreak();
			const _float fBrR = (fBrMax > 0.f) ? (fBrCur / fBrMax) : 0.f;
			m_fMonBreakFill = m_fMonBreakReduce = fBrR;
		}
		else
		{
			m_fMonBreakFill = m_fMonBreakReduce = 0.f;
		}
		m_fMonBreakReduceDelay = 0.f;
	}

	m_fSinceLastHit = 0.f;
	Set_MonsterBars_Visible(true);
}

void CHUD_GamePlay::Notify_Death(CMonster* pMonster)
{
	if (nullptr == pMonster)
		return;

	if (m_pCurrentTarget != pMonster)
		return;

	//  TODO : 0.5초 Short Fade OUT
	Safe_Release(m_pCurrentTarget);
	m_pCurrentTarget = nullptr;
	m_pLastTextTarget = nullptr;

	m_fSinceLastHit = 0.f;

	m_fMonHpFill = 0.f;
	m_fMonHpReduce = 0.f;
	m_fMonHpReduceDelay = 0.f;

	m_fMonBreakFill = 0.f;
	m_fMonBreakReduce = 0.f;
	m_fMonBreakReduceDelay = 0.f;

	Apply_Bar_Visuals(HUD_SLOT::MONSTER_HP_FILL, HUD_SLOT::MONSTER_HP_REDUCE, 0.f, 0.f);
	Apply_Bar_Visuals(HUD_SLOT::MONSTER_BREAK_FILL, HUD_SLOT::MONSTER_BREAK_REDUCE, 0.f, 0.f);

	Set_MonsterBars_Visible(false);
}

void CHUD_GamePlay::Notify_DashInput()
{
	m_bDashInput = true;
	m_fSinceDashInput = 0.f;
	Set_PlayerDash_Visible(true);
}

void CHUD_GamePlay::Notify_CombatInput()
{
	m_bCombatInput = true;
	m_fSinceCombatInput = 0.f;
	Set_PlayerBars_Visible(true);
}

void CHUD_GamePlay::Notify_ComboHit()
{
	++m_iComboCount;
	m_fComboDecayTimer = COMBO_DECAY_STEP;		// 마지막 타격 이후 2초 여유  ->  이후 2초마다 1단계씩 하락
	m_fComboHitBounce = HIT_BOUNCE_DURATION;
}

void CHUD_GamePlay::Notify_Crash()
{
	Ensure_CrashUIs();
	Set_CrashVisible(true);
	m_bCrashEffectPlaying = true;
	m_fCrashEffectElapsed = 0.f;
	Spawn_CrashAtlasEffect();
}

HRESULT CHUD_GamePlay::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CHUD_GamePlay::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	s_pInstance = this;
	return S_OK;
}

void CHUD_GamePlay::Priority_Update(_float fTimeDelta)
{
}

void CHUD_GamePlay::Update(_float fTimeDelta)
{
	if (false == m_bCached)
	{
		Cache_UIs();
		Resolve_Player();
		Cache_Viewport();
		Cache_Dash_Offsets();

		const HUD_SLOT eBoxSlots[] = {
			HUD_SLOT::SKILL_C_KEYBOX, HUD_SLOT::SKILL_F_KEYBOX, HUD_SLOT::SKILL_Q_KEYBOX,
			HUD_SLOT::SKILL_E_KEYBOX, HUD_SLOT::SKILL_R_KEYBOX,
				HUD_SLOT::QTE_KEYBOX,
		};
		for (HUD_SLOT eBox : eBoxSlots)
			if (CUI_Image* pBox = m_pUI[ETOUI(eBox)])
			{
				pBox->Set_SweepMode(UI_SWEEP_MODE::BOX);
				pBox->Set_Color(_float4{ 0.3f, 0.3f, 0.3f, 0.5f });
			}

		if (CUI_Image* pLine = m_pUI[ETOUI(HUD_SLOT::QUEST_UNDERLINE)])
		{
			pLine->Set_SweepMode(UI_SWEEP_MODE::FILL);
			pLine->Set_Color(_float4{ 1.f, 1.f, 1.f, 0.85f });
		}

		const HUD_SLOT eGaugeVSlots[] = {
			HUD_SLOT::SKILL_C_COOL, HUD_SLOT::SKILL_F_COOL, HUD_SLOT::SKILL_Q_COOL,
			HUD_SLOT::SKILL_E_COOL, HUD_SLOT::SKILL_R_COOL, HUD_SLOT::QTE_COOL,
		};
		for (HUD_SLOT eGV : eGaugeVSlots)
			if (CUI_Image* pGV = m_pUI[ETOUI(eGV)])
				pGV->Set_SweepMode(UI_SWEEP_MODE::GAUGE_V);
		Set_MonsterBars_Visible(false);
		Set_PlayerBars_Visible(false);
		Set_PlayerDash_Visible(false);
		if (nullptr != m_pPlayer)
		{
			m_eCachedWeaponId = m_pPlayer->Get_EquippedWeapon();
			Refresh_SkillIcons(m_eCachedWeaponId);
		}
		m_bCached = true;
	}

	if (m_bDashInput)
	{
		m_fSinceDashInput += fTimeDelta;
		if (m_fSinceDashInput >= DASH_VISIBLE_FOR)
			Set_PlayerDash_Visible(false);
	}

	if (m_bCombatInput)
	{
		m_fSinceCombatInput += fTimeDelta;
		if (m_fSinceCombatInput >= BARS_VISIBLE_FOR)
			Set_PlayerBars_Visible(false);
	}

	if (nullptr != m_pCurrentTarget)
		m_fSinceLastHit += fTimeDelta;

	Tick_MonsterBars(fTimeDelta);
	Tick_PlayerBars(fTimeDelta);
	Tick_Dash(fTimeDelta);
	Tick_Sweep(fTimeDelta);
	Tick_Skills(fTimeDelta);
	Tick_Combo(fTimeDelta);
	Tick_CrashEffect(fTimeDelta);
}

void CHUD_GamePlay::Late_Update(_float fTimeDelta)
{
}

HRESULT CHUD_GamePlay::Render()
{
	return S_OK;
}

void CHUD_GamePlay::Cache_UIs()
{
	Ensure_CrashUIs();
	static const _tchar* Names[static_cast<_uint>(HUD_SLOT::END)] = {
	  TEXT("HUD_MonsterHP_Back"), TEXT("HUD_MonsterHP_Reduce"), TEXT("HUD_MonsterHP_Fill"), TEXT("HUD_MonsterHP_BarLight"),
	  TEXT("HUD_MonsterBreak_Back"), TEXT("HUD_MonsterBreak_Reduce"), TEXT("HUD_MonsterBreak_Fill"),
	  TEXT("HUD_PlayerHP_Back"), TEXT("HUD_PlayerHP_Reduce"), TEXT("HUD_PlayerHP_Fill"), TEXT("HUD_PlayerHP_BarLight"),
	  TEXT("HUD_PlayerMP_Back"), TEXT("HUD_PlayerMP_Reduce"), TEXT("HUD_PlayerMP_Fill"), TEXT("HUD_PlayerMP_BarLight"),
	  TEXT("HUD_Dash_Base"), TEXT("HUD_Dash_Line"),
	  TEXT("HUD_Dash_Step1"), TEXT("HUD_Dash_Step1_Glow"),
	  TEXT("HUD_Dash_Step2"), TEXT("HUD_Dash_Step2_Glow"),
	  TEXT("HUD_Dash_Step3"), TEXT("HUD_Dash_Step3_Glow"),
	  TEXT("HUD_Skill_C_Base"), TEXT("HUD_Skill_C_Icon"), TEXT("HUD_Skill_C_Cool"),
	  TEXT("HUD_Skill_F_Base"), TEXT("HUD_Skill_F_Icon"), TEXT("HUD_Skill_F_Cool"),
	  TEXT("HUD_Skill_Q_Base"), TEXT("HUD_Skill_Q_Icon"), TEXT("HUD_Skill_Q_Cool"),
	  TEXT("HUD_Skill_E_Base"), TEXT("HUD_Skill_E_Icon"), TEXT("HUD_Skill_E_Cool"),
	  TEXT("HUD_Skill_R_Base"), TEXT("HUD_Skill_R_Icon"), TEXT("HUD_Skill_R_Cool"),
	  TEXT("HUD_Skill_C_KeyBox"), TEXT("HUD_Skill_F_KeyBox"), TEXT("HUD_Skill_Q_KeyBox"), TEXT("HUD_Skill_E_KeyBox"), TEXT("HUD_Skill_R_KeyBox"),
	  TEXT("HUD_QTE_Frame"), TEXT("HUD_QTE_Icon"), TEXT("HUD_QTE_KeyBox"), TEXT("HUD_QTE_Cool"), TEXT("HUD_QTE_Active"),
	  TEXT("HUD_Skill_C_Active"), TEXT("HUD_Skill_F_Active"), TEXT("HUD_Skill_Q_Active"), TEXT("HUD_Skill_E_Active"), TEXT("HUD_Skill_R_Active"),
	  TEXT("HUD_Quest_Alarm"), TEXT("HUD_Quest_Underline"),
	};

	for (_uint i = 0; i < static_cast<_uint>(HUD_SLOT::END); ++i)
		m_pUI[i] = Find_UI_ByName(Names[i]);

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (pLayers)
	{
		auto it = pLayers->find(TEXT("Layer_UI"));
		if (it != pLayers->end() && nullptr != it->second)
		{
			for (CGameObject* pObj : it->second->Get_GameObjects())
			{
				CUI_Text* pText = dynamic_cast<CUI_Text*>(pObj);
				if (nullptr == pText) continue;

				const _wstring& strName = pText->Get_ObjectName();
				if (strName == TEXT("HUD_MonsterLevel"))     
					m_pUI_MonsterLevel = pText;
				else if (strName == TEXT("HUD_MonsterName")) 
						m_pUI_MonsterName = pText;
					else if (strName == TEXT("HUD_Skill_C_Key")) 
						m_pSkillKeyText[0] = pText;
					else if (strName == TEXT("HUD_Skill_F_Key")) 
						m_pSkillKeyText[1] = pText;
					else if (strName == TEXT("HUD_Skill_Q_Key")) 
						m_pSkillKeyText[2] = pText;
					else if (strName == TEXT("HUD_Skill_E_Key")) 
						m_pSkillKeyText[3] = pText;
					else if (strName == TEXT("HUD_Skill_R_Key")) 
						m_pSkillKeyText[4] = pText;
					else if (strName == TEXT("HUD_QTE_Key")) 
						m_pSkillKeyText[5] = pText;
					else if (strName == TEXT("HUD_Quest_Title")) 
						m_pQuestText[0] = pText;
					else if (strName == TEXT("HUD_Quest_Objective")) 
						m_pQuestText[1] = pText;
					else if (strName == TEXT("HUD_Quest_Collect")) 
						m_pQuestText[2] = pText;
					else if (strName == TEXT("HUD_Quest_Collect")) 
						m_pQuestText[2] = pText;
					else if (strName == TEXT("HUD_Combo_Hits")) 
						m_pComboHitsText = pText;
			}
		}
	}

	m_pComboRank = dynamic_cast<CUI_SpriteAnim*>(Find_UI_ByName(TEXT("HUD_Combo_Rank")));
	m_pComboDigit[0] = dynamic_cast<CUI_SpriteAnim*>(Find_UI_ByName(TEXT("HUD_Combo_Digit0")));
	m_pComboDigit[1] = dynamic_cast<CUI_SpriteAnim*>(Find_UI_ByName(TEXT("HUD_Combo_Digit1")));
	m_pComboDigit[2] = dynamic_cast<CUI_SpriteAnim*>(Find_UI_ByName(TEXT("HUD_Combo_Digit2")));
	if (nullptr != m_pComboRank)
	{
		m_fComboRankBaseW = m_pComboRank->Get_SizeX();
		m_fComboRankBaseH = m_pComboRank->Get_SizeY();
	}
	for (_int i = 0; i < 3; ++i)
		if (nullptr != m_pComboDigit[i])
		{
			m_fComboDigitBaseX[i] = m_pComboDigit[i]->Get_CenterX();
			m_fComboDigitBaseY[i] = m_pComboDigit[i]->Get_CenterY();
		}
	if (nullptr != m_pComboHitsText)
	{
		m_fComboHitsBaseX = m_pComboHitsText->Get_CenterX();
		m_fComboHitsBaseY = m_pComboHitsText->Get_CenterY();
	}
}

void CHUD_GamePlay::Resolve_Player()
{
	if (nullptr == m_pGameInstance)
		return;

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return;

	auto it = pLayers->find(TEXT("Layer_Player"));
	if (it == pLayers->end() || nullptr == it->second)
		return;

	const auto& Objs = it->second->Get_GameObjects();
	if (Objs.empty())
		return;

	CPlayer* pPlayer = dynamic_cast<CPlayer*>(Objs.front());
	if (nullptr == pPlayer)
		return;

	Safe_AddRef(pPlayer);
	m_pPlayer = pPlayer;
}

void CHUD_GamePlay::Cache_Dash_Offsets()
{
	const HUD_SLOT eDashSlots[] = {
				HUD_SLOT::DASH_BASE, HUD_SLOT::DASH_LINE,
				HUD_SLOT::DASH_STEP1, HUD_SLOT::DASH_STEP1_GLOW,
				HUD_SLOT::DASH_STEP2, HUD_SLOT::DASH_STEP2_GLOW,
				HUD_SLOT::DASH_STEP3, HUD_SLOT::DASH_STEP3_GLOW,
	};

	for (HUD_SLOT eSlot : eDashSlots)
	{
		const _uint iIdx = ETOUI(eSlot);
		CUI_Image* pUI = m_pUI[iIdx];
		if (nullptr == pUI) continue;

		m_fDashOffsetX[iIdx] = pUI->Get_CenterX() - m_fViewW * 0.5f;
		m_fDashOffsetY[iIdx] = pUI->Get_CenterY() - m_fViewH * 0.5f;
	}
	m_bDashBaseCached = true;
}

void CHUD_GamePlay::Cache_Viewport()
{
	_uint           iNumViewport = 1;
	D3D11_VIEWPORT  Viewport = {};
	m_pContext->RSGetViewports(&iNumViewport, &Viewport);
	m_fViewW = Viewport.Width;
	m_fViewH = Viewport.Height;
}

CUI_Image* CHUD_GamePlay::Find_UI_ByName(const _tchar* pName)
{
	if (nullptr == pName || nullptr == m_pGameInstance)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::GAMEPLAY));
	if (nullptr == pLayers)
		return nullptr;

	auto it = pLayers->find(TEXT("Layer_UI"));
	if (it == pLayers->end() || nullptr == it->second)
		return nullptr;

	for (CGameObject* pObj : it->second->Get_GameObjects())
	{
		CUIObject* pUI = dynamic_cast<CUIObject*>(pObj);
		if (nullptr == pUI)
			continue;
		if (pUI->Get_ObjectName() == pName)
			return dynamic_cast<CUI_Image*>(pUI);
	}

	return nullptr;
}

void CHUD_GamePlay::Tick_LazyBar(_float& fFill, _float& fReduce, _float& fDelay, _float fTarget, _float fTimeDelta)
{
	if (fTarget < fFill)
	{
		fFill = fTarget;
		fDelay = REDUCE_HOLD;   
	}
	else if (fTarget > fFill)
	{
		fFill = fReduce = fTarget;
		fDelay = 0.f;
		return;
	}

	if (fReduce > fFill)
	{
		if (fDelay > 0.f)
		{
			fDelay -= fTimeDelta;
		}
		else
		{
			fReduce -= REDUCE_LERP_SPEED * fTimeDelta;
			if (fReduce < fFill) fReduce = fFill;
		}
	}
}

void CHUD_GamePlay::Tick_Sweep(_float fTimeDelta)
{
	// === BarLight Position-sweep ===
	m_fBarSweepTime += fTimeDelta;
	if (m_fBarSweepTime >= BAR_SWEEP_TOTAL)
		m_fBarSweepTime -= BAR_SWEEP_TOTAL;

	_float t = -1.f;   // -1 = 휴식 구간
	if (m_fBarSweepTime < BAR_SWEEP_DURATION)
		t = m_fBarSweepTime / BAR_SWEEP_DURATION;

	struct SWEEP_PAIR
	{
		HUD_SLOT eLight;
		HUD_SLOT eBack;
		_float   fFillRatio;
	};

	const SWEEP_PAIR Pairs[] = {
		{ HUD_SLOT::MONSTER_HP_BARLIGHT, HUD_SLOT::MONSTER_HP_BACK, m_fMonHpFill },
		{ HUD_SLOT::PLAYER_HP_BARLIGHT,  HUD_SLOT::PLAYER_HP_BACK,  m_fPlyHpFill },
		{ HUD_SLOT::PLAYER_MP_BARLIGHT,  HUD_SLOT::PLAYER_MP_BACK,  m_fPlyMpFill },
	};

	for (const SWEEP_PAIR& P : Pairs)
	{
		CUI_Image* pBack = m_pUI[ETOUI(P.eBack)];
		CUI_Image* pLight = m_pUI[ETOUI(P.eLight)];
		if (nullptr == pBack || nullptr == pLight) continue;

		const _float fBackHalf = pBack->Get_SizeX() * 0.5f;
		const _float fLightHalf = pLight->Get_SizeX() * 0.5f;
		const _float fBackLeft = pBack->Get_CenterX() - fBackHalf;
		const _float fFillEndX = fBackLeft + pBack->Get_SizeX() * P.fFillRatio;

		// 광원의 좌측 가장자리가 fill 시작점 = sweep 시작
		// 광원의 우측 가장자리가 fill 끝점   = sweep 종료
		const _float fStartCx = fBackLeft + fLightHalf;
		const _float fEndCx = fFillEndX - fLightHalf;

		// 휴식 구간 OR fill 이 광원 폭보다 작아 sweep 불가능 → 화면 밖
		if (t < 0.f || fEndCx <= fStartCx)
		{
			pLight->Set_Center(fStartCx - 10000.f, pLight->Get_CenterY());
			continue;
		}

		const _float fX = fStartCx + (fEndCx - fStartCx) * t;
		pLight->Set_Center(fX, pLight->Get_CenterY());
	}

	// === Glow UV-sweep ===
	m_fGlowSweepTime += fTimeDelta * GLOW_UV_SPEED;
	if (m_fGlowSweepTime >= 1.f)
		m_fGlowSweepTime -= floorf(m_fGlowSweepTime);

	static const HUD_SLOT GlowSlots[] = {
		HUD_SLOT::DASH_STEP1_GLOW,
		HUD_SLOT::DASH_STEP2_GLOW,
		HUD_SLOT::DASH_STEP3_GLOW,
	};

	for (HUD_SLOT eSlot : GlowSlots)
	{
		if (CUI_Image* pGlow = m_pUI[ETOUI(eSlot)])
			pGlow->Set_UVOffset(0.f, m_fGlowSweepTime);
	}

	// === Glow trigger ===
	if (m_pPlayer)
	{
		const _int iCharge = m_pPlayer->Get_DashCharge();
		m_bGlowTrigger[0] = (iCharge >= 1);
		m_bGlowTrigger[1] = (iCharge >= 2);
		m_bGlowTrigger[2] = (iCharge >= 3);
	}
}

void CHUD_GamePlay::Apply_Bar_Visuals(HUD_SLOT eFill, HUD_SLOT eReduce, _float fFill, _float fReduce)
{
	if (CUI_Image* pFill = m_pUI[ETOUI(eFill)])
		pFill->Set_GaugeRatio(fFill);
	if (CUI_Image* pReduce = m_pUI[ETOUI(eReduce)])
		pReduce->Set_GaugeRatio(fReduce);
}

void CHUD_GamePlay::Tick_MonsterBars(_float fTimeDelta)
{
	if (nullptr == m_pCurrentTarget)
		return;

	// HP
	{
		const _float fMax = m_pCurrentTarget->Get_MaxHP();
		const _float fCur = m_pCurrentTarget->Get_CurrentHP();
		const _float fTarget = (fMax > 0.f) ? (fCur / fMax) : 0.f;
		Tick_LazyBar(m_fMonHpFill, m_fMonHpReduce, m_fMonHpReduceDelay, fTarget, fTimeDelta);
		Apply_Bar_Visuals(HUD_SLOT::MONSTER_HP_FILL, HUD_SLOT::MONSTER_HP_REDUCE,
			m_fMonHpFill, m_fMonHpReduce);
	}

	// Break
	if (m_pCurrentTarget->Has_Break())
	{
		const _float fMax = m_pCurrentTarget->Get_MaxBreak();
		const _float fCur = m_pCurrentTarget->Get_CurrentBreak();
		const _float fTarget = (fMax > 0.f) ? (fCur / fMax) : 0.f;
		Tick_LazyBar(m_fMonBreakFill, m_fMonBreakReduce, m_fMonBreakReduceDelay, fTarget, fTimeDelta);
		Apply_Bar_Visuals(HUD_SLOT::MONSTER_BREAK_FILL, HUD_SLOT::MONSTER_BREAK_REDUCE,
			m_fMonBreakFill, m_fMonBreakReduce);
	}

	if (m_pLastTextTarget != m_pCurrentTarget)
	{
		m_pLastTextTarget = m_pCurrentTarget;

		if (nullptr != m_pCurrentTarget)
		{
			if (m_pUI_MonsterLevel)
			{
				_tchar szLevel[32] = {};
				swprintf_s(szLevel, TEXT("Lv. %d"), m_pCurrentTarget->Get_Level());
				m_pUI_MonsterLevel->Set_Text(szLevel);
			}
			if (m_pUI_MonsterName)
				m_pUI_MonsterName->Set_Text(m_pCurrentTarget->Get_DisplayName().c_str());
		}
	}
}

void CHUD_GamePlay::Tick_PlayerBars(_float fTimeDelta)
{
	if (nullptr == m_pPlayer)
		return;

	// HP
	{
		const _float fMax = m_pPlayer->Get_MaxHP();
		const _float fCur = m_pPlayer->Get_CurrentHP();
		const _float fTarget = (fMax > 0.f) ? (fCur / fMax) : 0.f;
		Tick_LazyBar(m_fPlyHpFill, m_fPlyHpReduce, m_fPlyHpReduceDelay, fTarget, fTimeDelta);
		Apply_Bar_Visuals(HUD_SLOT::PLAYER_HP_FILL, HUD_SLOT::PLAYER_HP_REDUCE,
			m_fPlyHpFill, m_fPlyHpReduce);
	}

	// MP
	{
		const _float fMax = m_pPlayer->Get_MaxMP();
		const _float fCur = m_pPlayer->Get_CurrentMP();
		const _float fTarget = (fMax > 0.f) ? (fCur / fMax) : 0.f;
		Tick_LazyBar(m_fPlyMpFill, m_fPlyMpReduce, m_fPlyMpReduceDelay, fTarget, fTimeDelta);
		Apply_Bar_Visuals(HUD_SLOT::PLAYER_MP_FILL, HUD_SLOT::PLAYER_MP_REDUCE,
			m_fPlyMpFill, m_fPlyMpReduce);
	}
}

void CHUD_GamePlay::Tick_Dash(_float fTimeDelta)
{
	if (false == m_bDashInput)
		return;

	if (nullptr == m_pPlayer || false == m_bDashBaseCached)
		return;

	CTransform* pPlayerTr = m_pPlayer->Get_Transform();
	if (nullptr == pPlayerTr)
		return;

	_float3 vDashWorldPos{};
	_vector vWorld = {};

	if (m_pPlayer->Try_GetDashHUDWorldPosition(&vDashWorldPos))
	{
		vWorld = XMLoadFloat3(&vDashWorldPos);
	}
	else
	{
		vWorld = pPlayerTr->Get_State(STATE::POSITION);
		vWorld = XMVectorSetY(vWorld, XMVectorGetY(vWorld) + 1.0f);
	}

	const _float4x4* pView = m_pGameInstance->Get_Transform(D3DTS::VIEW);
	const _float4x4* pProj = m_pGameInstance->Get_Transform(D3DTS::PROJ);
	if (nullptr == pView || nullptr == pProj) return;

	_matrix mViewProj = XMLoadFloat4x4(pView) * XMLoadFloat4x4(pProj);
	_vector vClip = XMVector3TransformCoord(vWorld, mViewProj);

	const _float fNdcX = XMVectorGetX(vClip);
	const _float fNdcY = XMVectorGetY(vClip);
	const _float fNdcZ = XMVectorGetZ(vClip);

	const _bool bBehind = (fNdcZ < 0.f) || (fNdcZ > 1.f);

	const _float fScreenX = (fNdcX * 0.5f + 0.5f) * m_fViewW;
	const _float fScreenY = (1.f - (fNdcY * 0.5f + 0.5f)) * m_fViewH;

	const _int iCharge = m_pPlayer->Get_DashCharge();
	const _int iChargeMax = m_pPlayer->Get_DashChargeMax();

	const _float fScaleX = m_fViewW / 1280.f;
	const _float fScaleY = m_fViewH / 720.f;

	const _float fDashCenterX = fScreenX + (-105.f * fScaleX);
	const _float fDashCenterY = fScreenY + (65.f * fScaleY);

	auto MoveSlot = [&](HUD_SLOT eSlot)
		{
			CUI_Image* pUI = m_pUI[ETOUI(eSlot)];
			if (nullptr == pUI)
				return;

			pUI->Set_Center(fDashCenterX, fDashCenterY);
		};

	auto SetVis = [&](HUD_SLOT eSlot, _bool bVis)
		{
			if (CUI_Image* pUI = m_pUI[ETOUI(eSlot)])
				pUI->Set_Visible(!bBehind && bVis);
		};

	MoveSlot(HUD_SLOT::DASH_BASE);
	MoveSlot(HUD_SLOT::DASH_LINE);
	MoveSlot(HUD_SLOT::DASH_STEP1);
	MoveSlot(HUD_SLOT::DASH_STEP1_GLOW);
	MoveSlot(HUD_SLOT::DASH_STEP2); 
	MoveSlot(HUD_SLOT::DASH_STEP2_GLOW);
	MoveSlot(HUD_SLOT::DASH_STEP3); 
	MoveSlot(HUD_SLOT::DASH_STEP3_GLOW);

	SetVis(HUD_SLOT::DASH_BASE, true);
	SetVis(HUD_SLOT::DASH_LINE, true);

	SetVis(HUD_SLOT::DASH_STEP1, iCharge >= 1);
	SetVis(HUD_SLOT::DASH_STEP2, iCharge >= 2);
	SetVis(HUD_SLOT::DASH_STEP3, iCharge >= 3);

	SetVis(HUD_SLOT::DASH_STEP1_GLOW, (iCharge >= 1) && m_bGlowTrigger[0]);
	SetVis(HUD_SLOT::DASH_STEP2_GLOW, (iCharge >= 2) && m_bGlowTrigger[1]);
	SetVis(HUD_SLOT::DASH_STEP3_GLOW, (iCharge >= 3) && m_bGlowTrigger[2]);

	(void)iChargeMax;
}



void CHUD_GamePlay::Tick_Skills(_float fTimeDelta)
{
	if (nullptr == m_pPlayer)
		return;

	const _bool bQTEWin = m_pPlayer->Is_QTEWindowActive();
	const _bool bQTECool = m_pPlayer->Is_QTEOnCooldown(QTE_TYPE::EXTREME_DASH);
	const _bool bQTEReady = bQTEWin && !bQTECool;

	if (bQTEWin)
	{
		m_bCombatInput = true;
		m_fSinceCombatInput = 0.f;
		Set_PlayerBars_Visible(true);
	}
	if (CUI_Image* pQF = m_pUI[ETOUI(HUD_SLOT::QTE_FRAME)]) 
		pQF->Set_Visible(bQTEWin);
	if (CUI_Image* pQA = m_pUI[ETOUI(HUD_SLOT::QTE_ACTIVE)]) 
		pQA->Set_Visible(bQTEReady);
	if (CUI_Image* pQI = m_pUI[ETOUI(HUD_SLOT::QTE_ICON)]) 
		pQI->Set_Visible(bQTEWin);
	if (CUI_Image* pQB = m_pUI[ETOUI(HUD_SLOT::QTE_KEYBOX)]) 
		pQB->Set_Visible(bQTEWin);
	if (m_pSkillKeyText[5]) 
		m_pSkillKeyText[5]->Set_Visible(bQTEReady);
	if (CUI_Image* pQC = m_pUI[ETOUI(HUD_SLOT::QTE_COOL)])
	{
		if (bQTEWin && bQTECool)
		{
			_float fQR = m_pPlayer->Get_QTECooldownTimer(QTE_TYPE::EXTREME_DASH);
			_float fQM = m_pPlayer->Get_QTECooldownMax(QTE_TYPE::EXTREME_DASH);
			pQC->Set_GaugeVertical(true);
			pQC->Set_GaugeRatio((fQM > 0.f) ? (1.f - fQR / fQM) : 1.f);
			pQC->Set_Visible(true);
		}
		else
			pQC->Set_Visible(false);
	}

	EQUIPPED_WEAPON_ID eWeapon = m_pPlayer->Get_EquippedWeapon();
	if (eWeapon != m_eCachedWeaponId)
	{
		Refresh_SkillIcons(eWeapon);
		m_eCachedWeaponId = eWeapon;
	}

	struct COOL_PAIR { HUD_SLOT eCool; HUD_SLOT eActive; _float fRemain; _float fMax; };
	const COOL_PAIR Pairs[] = {
		{ HUD_SLOT::SKILL_C_COOL, HUD_SLOT::SKILL_C_ACTIVE, m_pPlayer->Get_WeaponSwapCooldownTimer(),         m_pPlayer->Get_WeaponSwapCooldownMax() },
		{ HUD_SLOT::SKILL_F_COOL, HUD_SLOT::SKILL_F_ACTIVE, m_pPlayer->Get_SkillCooldownTimer(SKILL_SLOT::F), m_pPlayer->Get_SkillCooldownMax(SKILL_SLOT::F) },
		{ HUD_SLOT::SKILL_Q_COOL, HUD_SLOT::SKILL_Q_ACTIVE, m_pPlayer->Get_SkillCooldownTimer(SKILL_SLOT::Q), m_pPlayer->Get_SkillCooldownMax(SKILL_SLOT::Q) },
		{ HUD_SLOT::SKILL_E_COOL, HUD_SLOT::SKILL_E_ACTIVE, m_pPlayer->Get_SkillCooldownTimer(SKILL_SLOT::E), m_pPlayer->Get_SkillCooldownMax(SKILL_SLOT::E) },
		{ HUD_SLOT::SKILL_R_COOL, HUD_SLOT::SKILL_R_ACTIVE, m_pPlayer->Get_SkillCooldownTimer(SKILL_SLOT::R), m_pPlayer->Get_SkillCooldownMax(SKILL_SLOT::R) },
	};

	for (_int i = 0; i < 5; ++i)
	{
		const COOL_PAIR& P = Pairs[i];
		CUI_Image* pCool = m_pUI[ETOUI(P.eCool)];
		if (nullptr == pCool)
			continue;

		if (m_bCombatInput && P.fRemain > 0.f && P.fMax > 0.f)
		{
			pCool->Set_GaugeVertical(true);
			pCool->Set_GaugeRatio(1.f - (P.fRemain / P.fMax));
			pCool->Set_Visible(true);
		}
		else
		{
			pCool->Set_Visible(false);
		}

		if (m_fPrevSkillCooldown[i] > 0.f && P.fRemain <= 0.f)
			m_fSkillActiveFlash[i] = SKILL_ACTIVE_FLASH;
		if (m_fSkillActiveFlash[i] > 0.f)
			m_fSkillActiveFlash[i] -= fTimeDelta;
		m_fPrevSkillCooldown[i] = P.fRemain;

		if (CUI_Image* pAct = m_pUI[ETOUI(P.eActive)])
		{
			const _bool bReady = m_bCombatInput && !(P.fRemain > 0.f && P.fMax > 0.f);
			pAct->Set_Visible((4 == i) ? bReady : (m_bCombatInput && m_fSkillActiveFlash[i] > 0.f));
		}
	}
}

void CHUD_GamePlay::Refresh_SkillIcons(EQUIPPED_WEAPON_ID eWeapon)
{
	if (EQUIPPED_WEAPON_ID::NONE == eWeapon)
		return;

	const _uint iLevel = ETOUI(LEVEL::GAMEPLAY);
	const _bool bKasaka = (EQUIPPED_WEAPON_ID::KASAKA_VENOM_FANG == eWeapon);

	auto SetIcon = [&](HUD_SLOT eSlot, const _tchar* pProto)
	{
		if (CUI_Image* pUI = m_pUI[ETOUI(eSlot)])
			pUI->Set_TextureByProto(iLevel, pProto);
	};

	SetIcon(HUD_SLOT::SKILL_C_ICON, bKasaka ? TEXT("Prototype_Component_Texture_HUD_Skill_Icon_WpnKasaka") : TEXT("Prototype_Component_Texture_HUD_Skill_Icon_WpnKK"));
	SetIcon(HUD_SLOT::SKILL_F_ICON, bKasaka ? TEXT("Prototype_Component_Texture_HUD_Skill_Icon_F_Kasaka") : TEXT("Prototype_Component_Texture_HUD_Skill_Icon_F_KK"));
	SetIcon(HUD_SLOT::SKILL_E_ICON, bKasaka ? TEXT("Prototype_Component_Texture_HUD_Skill_Icon_E_Kasaka") : TEXT("Prototype_Component_Texture_HUD_Skill_Icon_E_KK"));
}

void CHUD_GamePlay::Tick_Combo(_float fTimeDelta)
{
	// 1) 2초마다 등급 한 단계씩 하락
	if (m_iComboCount > 0)
	{
		m_fComboDecayTimer -= fTimeDelta;
		if (m_fComboDecayTimer <= 0.f)
		{
			const _int iCurRank = Combo_RankFromCount(m_iComboCount);
			if (iCurRank <= 0)
				m_iComboCount = 0;                                   // D 밑이면 종료
			else
				m_iComboCount = COMBO_THRESHOLD[iCurRank - 1];       // 한 단계 아래 하한으로
			m_fComboDecayTimer = COMBO_DECAY_STEP;
		}
	}

	// 2) 등급 산정 -> 변경 시 팝업
	const _int iNewRank = Combo_RankFromCount(m_iComboCount);
	if (iNewRank != m_iComboRank)
	{
		m_iComboRank = iNewRank;
		if (iNewRank >= 0)
		{
			if (nullptr != m_pComboRank)
				m_pComboRank->Set_Frame(static_cast<_uint>(iNewRank));
			m_fRankPopTimer = RANK_POP_DURATION;
		}
	}

	const _bool bShow = (m_iComboRank >= 0 && m_iComboCount > 0);

	// 3) 등급 표시 + 팝 Scale (Center는 고정, 확대)
	if (nullptr != m_pComboRank)
	{
		m_pComboRank->Set_Visible(bShow);

		_float fScale = 1.f;
		if (m_fRankPopTimer > 0.f)
		{
			m_fRankPopTimer = max(0.f, m_fRankPopTimer - fTimeDelta);
			const _float p = 1.f - m_fRankPopTimer / RANK_POP_DURATION;   
			if (p < 0.45f)
				fScale = 0.2f + (RANK_POP_SCALE_MAX - 0.2f) * (p / 0.45f);
			else
				fScale = RANK_POP_SCALE_MAX + (1.f - RANK_POP_SCALE_MAX) * ((p - 0.45f) / 0.55f);
		}
		if (m_fComboRankBaseW > 0.f)
			m_pComboRank->Set_Size(m_fComboRankBaseW * fScale, m_fComboRankBaseH * fScale);
	}

	// 4) 터격시 바운스 오프셋 ( 타격 순간 최대 -> 복귀)
	_float fBounce = 0.f;
	if (m_fComboHitBounce > 0.f)
	{
		m_fComboHitBounce = max(0.f, m_fComboHitBounce - fTimeDelta);
		fBounce = m_fComboHitBounce / HIT_BOUNCE_DURATION;   // 1 → 0
	}

	// 5) 타격 횟수 숫자 ( 우측 정렬) - 왼쪽으로 살짝 바운스
	_int iVal = m_iComboCount;
	for (_int i = 0; i < 3; ++i)
	{
		if (nullptr == m_pComboDigit[i])
		{
			iVal /= 10;
			continue;
		}
		const _bool bDigitOn = bShow && (i == 0 || iVal > 0);
		m_pComboDigit[i]->Set_Visible(bDigitOn);
		if (bDigitOn)
		{
			m_pComboDigit[i]->Set_Frame(static_cast<_uint>(iVal % 10));
			m_pComboDigit[i]->Set_Center(m_fComboDigitBaseX[i] - DIGIT_BOUNCE_X * fBounce, m_fComboDigitBaseY[i]);
		}
		iVal /= 10;
	}

	// 6) HITS 글자 오른쪽으로 바운스
	if (nullptr != m_pComboHitsText)
	{
		m_pComboHitsText->Set_Visible(bShow);
		m_pComboHitsText->Set_Center(m_fComboHitsBaseX + HITS_BOUNCE_X * fBounce, m_fComboHitsBaseY);
	}
}
void CHUD_GamePlay::Ensure_CrashUIs()
{
	static const _tchar* CrashNames[4] =
	{
		TEXT("HUD_CrashLight"),
		TEXT("HUD_CrashWhite"),
		TEXT("HUD_CrashFont"),
		TEXT("HUD_CrashMask"),
	};

	static const _tchar* CrashTextureTags[4] =
	{
		TEXT("Prototype_Component_Texture_HUD_CrashLight"),
		TEXT("Prototype_Component_Texture_HUD_CrashWhite"),
		TEXT("Prototype_Component_Texture_HUD_CrashFont"),
		TEXT("Prototype_Component_Texture_HUD_CrashMask"),
	};

	static const _float2 CrashBaseSizes[4] =
	{
		_float2(520.f, 309.f),
		_float2(443.f, 96.f),
		_float2(443.f, 96.f),
		_float2(416.f, 66.f),
	};

	for (_uint i = 0; i < 4; ++i)
	{
		if (nullptr == m_pCrashUI[i])
			m_pCrashUI[i] = Find_UI_ByName(CrashNames[i]);
		if (nullptr != m_pCrashUI[i])
			continue;

		CUI_Image::UI_IMAGE_DESC Desc{};
		Desc.fCenterX = m_fViewW * 0.5f;
		Desc.fCenterY = m_fViewH * 0.43f;
		Desc.fSizeX = CrashBaseSizes[i].x;
		Desc.fSizeY = CrashBaseSizes[i].y;
		Desc.iZOrder = 8500 + i;
		Desc.pObjectName = CrashNames[i];
		Desc.bVisible = false;
		Desc.pTextureProtoTag = CrashTextureTags[i];
		Desc.iTextureProtoLevel = ETOUI(LEVEL::GAMEPLAY);
		Desc.vColor = _float4(1.f, 1.f, 1.f, 1.f);

		if (FAILED(m_pGameInstance->Add_GameObject(
			ETOUI(LEVEL::STATIC), TEXT("Prototype_GameObject_UI_Image"),
			ETOUI(LEVEL::GAMEPLAY), TEXT("Layer_UI"), &Desc)))
			continue;

		m_pCrashUI[i] = Find_UI_ByName(CrashNames[i]);
	}

	Set_CrashVisible(false);
}

void CHUD_GamePlay::Spawn_CrashAtlasEffect()
{
    const _float4x4* pView = m_pGameInstance->Get_Transform(D3DTS::VIEW);
    const _float4x4* pProj = m_pGameInstance->Get_Transform(D3DTS::PROJ);
    if (nullptr == pView || nullptr == pProj)
        return;

    constexpr _float fEffectDistance = 4.0f;
    _matrix ViewMatrix = XMLoadFloat4x4(pView);
    _matrix InvViewMatrix = XMMatrixInverse(nullptr, ViewMatrix);
    _vector vCameraPosition = InvViewMatrix.r[3];
    _vector vCameraLook = XMVector3Normalize(InvViewMatrix.r[2]);

    const _float fProjX = max(0.001f, pProj->_11);
    const _float fProjY = max(0.001f, pProj->_22);
    const _float fViewHeight = (2.f * fEffectDistance) / fProjY;
    const _float fViewWidth = fViewHeight * (fProjY / fProjX);

    vector<_float4> Positions;
    _float4 vEffectPosition{};
    XMStoreFloat4(&vEffectPosition, vCameraPosition + vCameraLook * fEffectDistance);
    vEffectPosition.w = 1.f;
    Positions.push_back(vEffectPosition);

    CAtlasInstanceEffect::ATLAS_INSTANCE_EFFECT_DESC Desc{};
    Desc.pTexturePrototypeTag = TEXT("Prototype_Component_Texture_Effect_Crash_Atlas");
    Desc.pPositions = &Positions;
    Desc.iMaxInstanceCount = 1;
    Desc.iAtlasCols = 4;
    Desc.iAtlasRows = 4;
    Desc.fFrameDuration = 0.035f;
    Desc.vSize = _float2(fViewWidth * 1.15f, fViewHeight * 1.15f);
    Desc.vUVPadding = _float2(0.001f, 0.001f);
    Desc.vColor = _float4(1.25f, 1.25f, 1.25f, 1.f);
    Desc.fAlpha = 0.95f;
    Desc.iShaderPass = 1;
    Desc.bLoop = false;

    m_pGameInstance->Add_GameObject(
        ETOUI(LEVEL::GAMEPLAY), TEXT("Prototype_GameObject_AtlasInstanceEffect"),
        ETOUI(LEVEL::GAMEPLAY), TEXT("Layer_Effect"), &Desc);
}

void CHUD_GamePlay::Tick_CrashEffect(_float fTimeDelta)
{
	if (false == m_bCrashEffectPlaying)
		return;

	m_fCrashEffectElapsed += fTimeDelta;
	const _float fT = min(1.f, m_fCrashEffectElapsed / CRASH_EFFECT_DURATION);

	_float fScale = 1.f;
	if (fT < 0.18f)
		fScale = 1.55f + (1.08f - 1.55f) * (fT / 0.18f);
	else if (fT < 0.32f)
		fScale = 1.08f + (1.f - 1.08f) * ((fT - 0.18f) / 0.14f);

	const _float fFade = (fT < 0.65f) ? 1.f : max(0.f, 1.f - ((fT - 0.65f) / 0.35f));
	const _float fLightAlpha = max(0.f, 1.f - fT);
	const _float fWhiteAlpha = (fT < 0.18f) ? (1.f - fT / 0.18f) : 0.f;
	const _float fMaskAlpha = (fT < 0.35f) ? 0.7f : max(0.f, 0.7f * (1.f - ((fT - 0.35f) / 0.25f)));

	static const _float2 CrashBaseSizes[4] =
	{
		_float2(520.f, 309.f),
		_float2(443.f, 96.f),
		_float2(443.f, 96.f),
		_float2(416.f, 66.f),
	};

	const _float fCenterX = m_fViewW * 0.5f;
	const _float fCenterY = m_fViewH * 0.43f;
	const _float Alphas[4] = { fLightAlpha, fWhiteAlpha, fFade, fMaskAlpha };

	for (_uint i = 0; i < 4; ++i)
	{
		if (nullptr == m_pCrashUI[i])
			continue;

		//const _float fLayerScale = (0 == i) ? (fScale * 1.12f) : fScale;

		const _float fLayerScale = fScale;

		m_pCrashUI[i]->Set_Center(fCenterX, fCenterY);

		if (0 == i)
			m_pCrashUI[i]->Set_Size(m_fViewW * 1.15f, m_fViewH * 1.15f);
		else
			m_pCrashUI[i]->Set_Size(CrashBaseSizes[i].x * fLayerScale, CrashBaseSizes[i].y * fLayerScale);

		m_pCrashUI[i]->Set_Alpha(Alphas[i]);
		m_pCrashUI[i]->Set_Visible(Alphas[i] > 0.01f);
	}

	if (CRASH_EFFECT_DURATION <= m_fCrashEffectElapsed)
	{
		m_bCrashEffectPlaying = false;
		Set_CrashVisible(false);
	}
}

void CHUD_GamePlay::Set_CrashVisible(_bool bVisible)
{
	for (_uint i = 0; i < 4; ++i)
	{
		if (nullptr == m_pCrashUI[i])
			continue;
		m_pCrashUI[i]->Set_Visible(bVisible);
		if (false == bVisible)
			m_pCrashUI[i]->Set_Alpha(0.f);
	}
}

_int CHUD_GamePlay::Combo_RankFromCount(_int iCount) const
{
	_int iRank = -1;
	for (_int i = 0; i < 7; ++i)
		if (iCount >= COMBO_THRESHOLD[i])
			iRank = i;
	return iRank;
}

void CHUD_GamePlay::Set_MonsterBars_Visible(_bool bVisible)
{
	const HUD_SLOT eSlots[] = {
				HUD_SLOT::MONSTER_HP_BACK,   HUD_SLOT::MONSTER_HP_REDUCE,    HUD_SLOT::MONSTER_HP_FILL,
				HUD_SLOT::MONSTER_HP_BARLIGHT,
				HUD_SLOT::MONSTER_BREAK_BACK,HUD_SLOT::MONSTER_BREAK_REDUCE, HUD_SLOT::MONSTER_BREAK_FILL,
	};

	for (HUD_SLOT eSlot : eSlots)
	{
		if (CUI_Image* pUI = m_pUI[ETOUI(eSlot)])
			pUI->Set_Visible(bVisible);
	}

	if (m_pUI_MonsterLevel) 
		m_pUI_MonsterLevel->Set_Visible(bVisible);

	if (m_pUI_MonsterName)  
		m_pUI_MonsterName->Set_Visible(bVisible);
}

void CHUD_GamePlay::Set_PlayerBars_Visible(_bool bVisible)
{
	if (!bVisible)
	{
		m_bCombatInput = false;
		m_fSinceCombatInput = 0.f;
	}

	for (_int i = 0; i < 5; ++i)
		if (m_pSkillKeyText[i]) m_pSkillKeyText[i]->Set_Visible(bVisible);

	for (_int i = 0; i < 3; ++i)
		if (m_pQuestText[i]) m_pQuestText[i]->Set_Visible(bVisible);

	const HUD_SLOT eSlots[] = {
				HUD_SLOT::PLAYER_HP_BACK, HUD_SLOT::PLAYER_HP_REDUCE, HUD_SLOT::PLAYER_HP_FILL,
				HUD_SLOT::PLAYER_HP_BARLIGHT,
				HUD_SLOT::PLAYER_MP_BACK, HUD_SLOT::PLAYER_MP_REDUCE, HUD_SLOT::PLAYER_MP_FILL,
				HUD_SLOT::PLAYER_MP_BARLIGHT,
				HUD_SLOT::SKILL_C_BASE, HUD_SLOT::SKILL_C_ICON,
				HUD_SLOT::SKILL_F_BASE, HUD_SLOT::SKILL_F_ICON,
				HUD_SLOT::SKILL_Q_BASE, HUD_SLOT::SKILL_Q_ICON,
				HUD_SLOT::SKILL_E_BASE, HUD_SLOT::SKILL_E_ICON,
				HUD_SLOT::SKILL_R_BASE, HUD_SLOT::SKILL_R_ICON,
				HUD_SLOT::SKILL_C_KEYBOX, HUD_SLOT::SKILL_F_KEYBOX, HUD_SLOT::SKILL_Q_KEYBOX, HUD_SLOT::SKILL_E_KEYBOX, HUD_SLOT::SKILL_R_KEYBOX,
				HUD_SLOT::QUEST_ALARM, HUD_SLOT::QUEST_UNDERLINE,
	};

	for (HUD_SLOT eSlot : eSlots)
	{
		if (CUI_Image* pUI = m_pUI[ETOUI(eSlot)])
			pUI->Set_Visible(bVisible);
	}
}

void CHUD_GamePlay::Set_PlayerDash_Visible(_bool bVisible)
{
	if (!bVisible)
	{
		m_bDashInput = false;
		m_fSinceDashInput = 0.f;

		for (_int i = 0; i < 3; ++i)
		{
			m_bGlowTrigger[i] = false;
		}
	}

	const HUD_SLOT eSlots[] = {
			HUD_SLOT::DASH_BASE, HUD_SLOT::DASH_LINE,
			HUD_SLOT::DASH_STEP1, HUD_SLOT::DASH_STEP1_GLOW,
			HUD_SLOT::DASH_STEP2, HUD_SLOT::DASH_STEP2_GLOW,
			HUD_SLOT::DASH_STEP3, HUD_SLOT::DASH_STEP3_GLOW,
	};

	for (HUD_SLOT eSlot : eSlots)
	{
		if (CUI_Image* pUI = m_pUI[ETOUI(eSlot)])
			pUI->Set_Visible(bVisible);
	}
}

CHUD_GamePlay* CHUD_GamePlay::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CHUD_GamePlay* pInstance = new CHUD_GamePlay(pDevice, pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CHUD_GamePlay");
		Safe_Release(pInstance);
	}
	return pInstance;
}

CGameObject* CHUD_GamePlay::Clone(void* pArg)
{
	CHUD_GamePlay* pInstance = new CHUD_GamePlay(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CHUD_GamePlay");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void CHUD_GamePlay::Free()
{
	__super::Free();

	if (s_pInstance == this)
		s_pInstance = nullptr;

	Safe_Release(m_pCurrentTarget);
	Safe_Release(m_pPlayer);
}
