#include "Level_Logo.h"
#include "Level_Loading.h"
#include "GameInstance.h"
#include "UISceneLoader.h"
#include "UI_Image.h"
#include "UI_Text.h"
#include "FadeOverlay_Helper.h"
#include "Layer.h"

CLevel_Logo::CLevel_Logo(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CLevel{ pDevice , pContext }
{
}

HRESULT CLevel_Logo::Initialize()
{
	if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
		return E_FAIL;

	Cache_UIElements();
    Cache_MenuBtnBaseColors();
	Enter_Title();

    if (CUI_Image* pFade = CFadeOverlay_Helper::Find())
    {
        pFade->Set_Alpha(1.f);
        pFade->Start_Fade(0.f, 0.5f);           // 0.5초간 투명화
    }

	return S_OK;
}

void CLevel_Logo::Update(_float fTimeDelta)
{
	switch (m_eState)
	{
	case LOGO_STATE::TITLE:
		Update_Title(fTimeDelta);
		break;
	case LOGO_STATE::MENU:
		Update_Menu(fTimeDelta);
		break;
	default:
		break;
	}
}

HRESULT CLevel_Logo::Render()
{
	return S_OK;
}

HRESULT CLevel_Logo::Ready_Layer_BackGround(const _wstring& strLayerTag)
{
	if (FAILED(m_pGameInstance->Add_GameObject(ETOUI(LEVEL::LOGO), TEXT("Prototype_GameObject_BackGround"),
		ETOUI(LEVEL::LOGO), strLayerTag)))
		return E_FAIL;

	return S_OK;
}

HRESULT CLevel_Logo::Ready_Layer_UI(const _wstring& strLayerTag)
{
	return CUISceneLoader::Load_Into_Layer(
		TEXT("../../Resources/Scenes/UI/Logo.uiscene"),
		ETOUI(LEVEL::LOGO),
		strLayerTag);
}

void CLevel_Logo::Cache_UIElements()
{
    const auto* pLayers = m_pGameInstance->Get_Layers(ETOUI(LEVEL::LOGO));
    if (nullptr == pLayers)
        return;

    auto it = pLayers->find(TEXT("Layer_UI"));
    if (it == pLayers->end())
        return;

    for (CGameObject* pObj : it->second->Get_GameObjects())
    {
        CUIObject* pUI = dynamic_cast<CUIObject*>(pObj);

        if (!pUI) 
            continue;

        const _wstring& strName = pUI->Get_ObjectName();

        if (strName == TEXT("Title_PressAnyKey"))
        {
            m_pPressAnyKey = dynamic_cast<CUI_Text*>(pUI);
            Safe_AddRef(m_pPressAnyKey);
        }
        else if (strName == TEXT("Menu_BtnStart"))
        {
            m_pBtnStart = dynamic_cast<CUI_Text*>(pUI);
            Safe_AddRef(m_pBtnStart);
        }
        else if (strName == TEXT("Menu_BtnOptions"))
        {
            m_pBtnOptions = dynamic_cast<CUI_Text*>(pUI);
            Safe_AddRef(m_pBtnOptions);
        }
        else if (strName == TEXT("Menu_BtnQuit"))
        {
            m_pBtnQuit = dynamic_cast<CUI_Text*>(pUI);
            Safe_AddRef(m_pBtnQuit);
        }
        else if (strName == TEXT("Menu_Panel_Bg"))
        {
            m_pMenuPanelBg = dynamic_cast<CUI_Image*>(pUI);
            Safe_AddRef(m_pMenuPanelBg);
        }
    }
}

void CLevel_Logo::Cache_MenuBtnBaseColors()
{
    for (_uint i = 0; i < MENU_ITEM_COUNT; ++i)
        m_vMenuBtnBaseColor[i] = _float4{ 1.f, 1.f, 1.f, 1.f };

    for (_uint i = 0; i < MENU_ITEM_COUNT; ++i)
    {
        MENU_ITEM eItem = static_cast<MENU_ITEM>(i);

        if (CUI_Text* pButton = Get_MenuButton(eItem))
            m_vMenuBtnBaseColor[i] = pButton->Get_Color();
    }
}

_float4 CLevel_Logo::Get_MenuButtonBaseColor(MENU_ITEM eItem) const
{
    const _uint iIndex = ETOUI(eItem);

    if (iIndex >= MENU_ITEM_COUNT)
        return _float4{ 1.f, 1.f, 1.f, 1.f };

    return m_vMenuBtnBaseColor[iIndex];
}

void CLevel_Logo::Enter_Title()
{
    m_eState = LOGO_STATE::TITLE;

    if (m_pPressAnyKey)
    {
        m_pPressAnyKey->Set_Visible(true);
        m_pPressAnyKey->Set_Scale(m_fPressBaseScale);
        m_pPressAnyKey->Set_Visible(true);
        m_pPressAnyKey->Set_EffectScale(1.f);
    }

    m_fPressBounceTime = 0.f;

    if (m_pBtnStart)     
        m_pBtnStart->Set_Visible(false);
    if (m_pBtnOptions)   
        m_pBtnOptions->Set_Visible(false);
    if (m_pBtnQuit)      
        m_pBtnQuit->Set_Visible(false);
    if (m_pMenuPanelBg)  
        m_pMenuPanelBg->Set_Visible(false);

    m_pGameInstance->Play_BGM(TEXT("Bgm_Title.wav"), 0.8f, true);
}

void CLevel_Logo::Enter_Menu()
{
    m_pGameInstance->Play_Sound(TEXT("Button_Title.wav"),
                                 SOUND_CHANNEL::UI,
                                 1.0f,
                                 false);

    m_eState = LOGO_STATE::MENU;
    m_eHovered = MENU_ITEM::END;

    if (m_pPressAnyKey)
    {
        m_pPressAnyKey->Stop_AlphaPulse();
        m_pPressAnyKey->Set_Scale(m_fPressBaseScale);
        m_pPressAnyKey->Set_Visible(false);
        m_pPressAnyKey->Set_EffectScale(1.f);
    }

    if (m_pBtnStart)     
        m_pBtnStart->Set_Visible(true);
    if (m_pBtnOptions)   
        m_pBtnOptions->Set_Visible(true);
    if (m_pBtnQuit)      
        m_pBtnQuit->Set_Visible(true);
    if (m_pMenuPanelBg)  
        m_pMenuPanelBg->Set_Visible(true);
}

void CLevel_Logo::Update_Title(_float fTimeDelta)
{
    Update_PressAnyKeyBounce(fTimeDelta);

    // 키보드/마우스 입력으로 MENU 상태 진입
    if (m_pGameInstance->Get_KeyDown(VK_RETURN) ||
        m_pGameInstance->Get_KeyDown(VK_SPACE) ||
        m_pGameInstance->Get_MouseBtnDown(MOUSEBTN::LBUTTON))
    {
        Enter_Menu();
    }
}

void CLevel_Logo::Update_Menu(_float fTimeDelta)
{
    if (m_pGameInstance->Get_KeyDown(VK_ESCAPE))
    {
        Set_Hovered(MENU_ITEM::END);
        Enter_Title();
        return;
    }

    _float fMouseX{}, fMouseY{};
    if (!Get_MouseCanvasPos(&fMouseX, &fMouseY))
        return;

    // hover 판정
    MENU_ITEM eHit = MENU_ITEM::END;

    if (m_pBtnStart && m_pBtnStart->Is_Hovered(fMouseX, fMouseY))
        eHit = MENU_ITEM::START;
    else if (m_pBtnOptions && m_pBtnOptions->Is_Hovered(fMouseX, fMouseY))
        eHit = MENU_ITEM::OPTIONS;
    else if (m_pBtnQuit && m_pBtnQuit->Is_Hovered(fMouseX, fMouseY))
        eHit = MENU_ITEM::QUIT;

    if (eHit != m_eHovered)
        Set_Hovered(eHit);

    // 클릭 -> 액션
    if (m_pGameInstance->Get_MouseBtnDown(MOUSEBTN::LBUTTON) &&
        eHit != MENU_ITEM::END)
    {
        Dispatch_Action(eHit);
    }

}

_bool CLevel_Logo::Get_MouseCanvasPos(_float* pOutX, _float* pOutY)
{
    if (nullptr == pOutX || nullptr == pOutY)
        return false;

    POINT pt{};
    if (!GetCursorPos(&pt))
        return false;

    HWND hWnd = m_pGameInstance->Get_hWnd();
    if (!ScreenToClient(hWnd, &pt))
        return false;

    // 백버퍼 좌표 → uiscene 저작 좌표 (1280×720) 로 역변환 필요시 추후 추가.
    // 현재 UIObject 의 m_fCenterX/Y 는 백버퍼 좌표계 (UISceneLoader 가 스케일 적용).
    // 따라서 클라이언트 좌표 그대로 Is_Hovered 에 전달 가능.
    *pOutX = static_cast<_float>(pt.x);
    *pOutY = static_cast<_float>(pt.y);
    return true;
}

CUI_Text* CLevel_Logo::Get_MenuButton(MENU_ITEM eItem)
{
    switch (eItem)
    {
    case MENU_ITEM::START:   
        return m_pBtnStart;
    case MENU_ITEM::OPTIONS: 
        return m_pBtnOptions;
    case MENU_ITEM::QUIT:    
        return m_pBtnQuit;
    default:                 
        return nullptr;
    }
}

void CLevel_Logo::Set_Hovered(MENU_ITEM eNew)
{
    if (CUI_Text* pPrev = Get_MenuButton(m_eHovered))
    {
        pPrev->Stop_AlphaPulse();
        pPrev->Set_Color(Get_MenuButtonBaseColor(m_eHovered));
    }

    m_eHovered = eNew;

    if (CUI_Text* pNew = Get_MenuButton(m_eHovered))
    {
        m_pGameInstance->Play_Sound(TEXT("Button_Hovered.wav"), 
                                    SOUND_CHANNEL::UI,
                                    0.5f, 
                                    false);

        _float4 vHoverColor = Get_MenuButtonBaseColor(m_eHovered);
         
        vHoverColor.w = 1.f;

        pNew->Set_Color(vHoverColor);

        pNew->Set_AlphaPulse(0.75f, 0.62f, 1.f);
    }
}

void CLevel_Logo::Dispatch_Action(MENU_ITEM eItem)
{
    m_pGameInstance->Play_Sound(TEXT("Button_Enter.wav"),
        SOUND_CHANNEL::UI,
        0.6f,
        false);

    switch (eItem)
    {
    case MENU_ITEM::START:
    {
        m_pGameInstance->Stop_Sound(SOUND_CHANNEL::BGM);

        if (CUI_Image* pFade = CFadeOverlay_Helper::Find())
        {
            pFade->Start_Fade(1.f, 0.5f, [this]() {
                CLevel* pLoading = CLevel_Loading::Create(m_pDevice, m_pContext, LEVEL::GAMEPLAY);
                if (nullptr != pLoading)
                    m_pGameInstance->Change_Level(ETOI(LEVEL::LOADING), pLoading);
                });
        }
        else
        {
            CLevel* pLoading = CLevel_Loading::Create(m_pDevice, m_pContext, LEVEL::GAMEPLAY);
            if (nullptr != pLoading)
                m_pGameInstance->Change_Level(ETOI(LEVEL::LOADING), pLoading);
        }
        break;
    }
    case MENU_ITEM::OPTIONS:
    {
        OutputDebugString(TEXT("[Logo] Options clicked (미구현)\n"));
        break;
    }
    case MENU_ITEM::QUIT:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        break;
    }
}

void CLevel_Logo::Update_PressAnyKeyBounce(_float fTimeDelta)
{
    if (nullptr == m_pPressAnyKey)
        return;

    m_fPressBounceTime += fTimeDelta;

    constexpr _float fPeriod = 1.f;       // 한 번 튀는 주기
    constexpr _float fBounceRatio = 0.34f;  // 주기 중 실제 튀는 구간
    constexpr _float fAmplitude = 0.045f;   // 최대 4.5% 확대

    _float fPhase = fmodf(m_fPressBounceTime, fPeriod) / fPeriod;

    _float fBounce = 0.f;
    if (fPhase < fBounceRatio)
    {
        const _float fLocal = fPhase / fBounceRatio;

        // 0 -> 1 -> 0
        fBounce = sinf(fLocal * XM_PI);
    }

    const _float fEffectScale = 1.f + fAmplitude * fBounce;
    m_pPressAnyKey->Set_EffectScale(fEffectScale);
}

CLevel_Logo* CLevel_Logo::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CLevel_Logo* pInstance = new CLevel_Logo(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CLevel_Logo");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CLevel_Logo::Free()
{
	__super::Free();

    Safe_Release(m_pPressAnyKey);
    Safe_Release(m_pBtnStart);
    Safe_Release(m_pBtnOptions);
    Safe_Release(m_pBtnQuit);
    Safe_Release(m_pMenuPanelBg);
}