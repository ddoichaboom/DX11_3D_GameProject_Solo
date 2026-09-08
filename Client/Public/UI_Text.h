#pragma once

#include "Client_Defines.h"
#include "UIObject.h"

NS_BEGIN(Client)

class CLIENT_DLL CUI_Text : public CUIObject
{
public:
    typedef struct tagUI_TextDesc : public CUIObject::UIOBJECT_DESC
    {
        const _tchar* pFontTag = { nullptr };
        const _tchar* pText = { TEXT("") };
        _float          fScale = { 1.f };
        _float4         vColor = { 1.f, 1.f, 1.f, 1.f };
        _float          fRotation = { 0.f };
        UI_TEXT_HALIGN  eHAlign = { UI_TEXT_HALIGN::CENTER };
        UI_TEXT_VALIGN  eVAlign = { UI_TEXT_VALIGN::MIDDLE };
        _bool           bAutoFit = { false };
    }UI_TEXT_DESC;

protected:
    CUI_Text(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CUI_Text(const CUI_Text& Prototype);
    virtual ~CUI_Text() = default;

public:
    virtual HRESULT         Initialize_Prototype() override;
    virtual HRESULT         Initialize(void* pArg) override;
    virtual void            Update(_float fTimeDelta) override;
    virtual void            Late_Update(_float fTimeDelta) override;
    virtual HRESULT         Render() override;

public:
    void                    Set_Text(const _tchar* pText) { m_strText = (pText ? pText : TEXT("")); }

    const                   _float4& Get_Color() const { return m_vColor; }
    void                    Set_Color(const _float4& vColor) { m_vColor = vColor; }
    void                    Set_Scale(_float fScale) { m_fScale = fScale; }

    void                    Set_Center(_float fCenterX, _float fCenterY);
    _float                  Get_CenterX() const { return m_fCenterX; }
    _float                  Get_CenterY() const { return m_fCenterY; }
    
    void                    Set_EffectScale(_float fScale) { m_fEffectScale = fScale; }
    _float                  Get_EffectScale() const { return m_fEffectScale; }
    void                    Set_Rotation(_float fRotation) { m_fRotation = fRotation; }

    void                    Set_AlphaPulse(_float fSpeed, _float fMinAlpha, _float fMaxAlpha);
    void                    Stop_AlphaPulse() 
    { 
        m_bAlphaPulse = false;
        m_fPulseTime = 0.f;
    }
    _bool                   Is_AlphaPulsing() const { return m_bAlphaPulse; }

private:
    _wstring                m_strFontTag;
    _wstring                m_strText;
    _float                  m_fScale = { 1.f };
    _float4                 m_vColor = { 1.f, 1.f, 1.f, 1.f };
    _float                  m_fRotation = { 0.f };
    UI_TEXT_HALIGN          m_eHAlign = { UI_TEXT_HALIGN::CENTER };
    UI_TEXT_VALIGN          m_eVAlign = { UI_TEXT_VALIGN::MIDDLE };
    _bool                   m_bAutoFit = { false };

    _bool                   m_bAlphaPulse = { false };
    _float                  m_fPulseSpeed = { 1.f };
    _float                  m_fPulseMin = { 0.3f };
    _float                  m_fPulseMax = { 1.f };
    _float                  m_fPulseTime = { 0.f };

    _float                  m_fEffectScale = { 1.f };

public:
    static CUI_Text*        Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CGameObject*    Clone(void* pArg) override;
    virtual void            Free() override;
};

NS_END

