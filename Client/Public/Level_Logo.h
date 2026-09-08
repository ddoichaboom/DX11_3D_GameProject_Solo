#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Client)
class CUI_Text;
class CUI_Image;

class CLevel_Logo final : public CLevel
{
private:
	CLevel_Logo(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CLevel_Logo() = default;

public:
	virtual HRESULT				Initialize() override;
	virtual void				Update(_float fTimeDelta) override;
	virtual HRESULT				Render() override;

public:
	HRESULT						Ready_Layer_BackGround(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_UI(const _wstring& strLayerTag);
	
private:
	void                        Cache_UIElements();
	void						Cache_MenuBtnBaseColors();
	_float4						Get_MenuButtonBaseColor(MENU_ITEM eItem) const;
	void                        Enter_Title();
	void                        Enter_Menu();
	void                        Update_Title(_float fTimeDelta);
	void                        Update_Menu(_float fTimeDelta);
	_bool                       Get_MouseCanvasPos(_float* pOutX, _float* pOutY);

	CUI_Text*					Get_MenuButton(MENU_ITEM eItem);
	void                        Set_Hovered(MENU_ITEM eNew);

	void                        Dispatch_Action(MENU_ITEM eItem);

	void						Update_PressAnyKeyBounce(_float fTimeDelta);

private:
	LOGO_STATE                  m_eState = { LOGO_STATE::TITLE };
	MENU_ITEM					m_eHovered = { MENU_ITEM::END };

	static constexpr _uint		MENU_ITEM_COUNT = ETOUI(MENU_ITEM::END);

	_float4						m_vMenuBtnBaseColor[MENU_ITEM_COUNT] = { };

	CUI_Text*					m_pPressAnyKey = { nullptr };
	CUI_Text*					m_pBtnStart = { nullptr };
	CUI_Text*					m_pBtnOptions = { nullptr };
	CUI_Text*					m_pBtnQuit = { nullptr };
	CUI_Image*					m_pMenuPanelBg = { nullptr };

	_float						m_fPressBounceTime = { 0.f };
	_float						m_fPressBaseScale = { 1.f };

public:
	static CLevel_Logo*			Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void				Free() override;
};

NS_END