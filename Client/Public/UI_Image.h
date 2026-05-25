#pragma once
#include "Client_Defines.h"
#include "UIObject.h"

NS_BEGIN(Engine)
class CShader;
class CVIBuffer;
class CTexture;
NS_END

NS_BEGIN(Client)

class CUI_Image : public CUIObject
{
public:
	typedef struct tagUI_ImageDesc : public CUIObject::UIOBJECT_DESC
	{
		const _tchar*	pTextureProtoTag = { nullptr };
		_uint			iTextureProtoLevel = { ETOUI(LEVEL::STATIC) };

		const _tchar*	pTexturePath = { nullptr };
		_float4			vColor = { 1.f, 1.f, 1.f, 1.f };
		UI_SWEEP_MODE	eSweepMode = { UI_SWEEP_MODE::NONE };
	}UI_IMAGE_DESC;

protected:
	CUI_Image(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CUI_Image(const CUI_Image& Prototype);
	virtual ~CUI_Image() = default;

public:
	void                        Set_Color(const _float4& vColor) { m_vColor = vColor; }
	const _float4&				Get_Color() const { return m_vColor; }

	void                        Set_SweepMode(UI_SWEEP_MODE eMode) { m_eSweepMode = eMode; }
	UI_SWEEP_MODE               Get_SweepMode() const { return m_eSweepMode; }

	void                        Set_UVOffset(_float fX, _float fY) { m_vUVOffset.x = fX; m_vUVOffset.y = fY; }
	const _float4&				Get_UVOffset() const { return m_vUVOffset; }

	void                        Set_GaugeRatio(_float fRatio);
	_float                      Get_GaugeRatio() const { return m_fGaugeProgress; }

	void                        Set_GaugeVertical(_bool bVertical) { m_bGaugeVertical = bVertical; }
	_bool                       Is_GaugeVertical() const { return m_bGaugeVertical; }

	HRESULT                     Set_TextureByProto(_uint iLevel, const _tchar* pProtoTag);

	void                        Set_Center(_float fCenterX, _float fCenterY);
	void                        Set_Size(_float fSizeX, _float fSizeY);

	_float                      Get_CenterX() const { return m_fCenterX; }
	_float                      Get_CenterY() const { return m_fCenterY; }
	_float                      Get_SizeX() const { return m_fSizeX; }
	_float                      Get_SizeY() const { return m_fSizeY; }

	void						Set_Alpha(_float fAlpha) { m_fAlpha = fAlpha; }
	_float                      Get_Alpha() const { return m_fAlpha; }
	_bool                       Is_Fading() const { return m_bFading; }

	void                        Start_Fade(_float fTargetAlpha, _float fDuration, function<void()> OnComplete = nullptr);
	void                        Stop_Fade();

public:
	virtual HRESULT				Initialize_Prototype() override;
	virtual HRESULT				Initialize(void* pArg) override;
	virtual void				Update(_float fTimeDelta) override;
	virtual void				Late_Update(_float fTimeDelta) override;
	virtual HRESULT				Render() override;

protected:
	CShader*					m_pShaderCom = { nullptr };
	CVIBuffer*					m_pVIBufferCom = { nullptr };
	CTexture*					m_pTextureCom = { nullptr };

	_float                      m_fAlpha = { 1.f };
	_float						m_fGaugeProgress = { 1.f };
	_bool                       m_bGaugeVertical = { false };
	_float4                     m_vColor = { 1.f, 1.f, 1.f, 1.f };
	UI_SWEEP_MODE               m_eSweepMode = { UI_SWEEP_MODE::NONE };
	_float4                     m_vUVOffset = { 0.f, 0.f, 0.f, 0.f };

	_bool                       m_bFading = { false };
	_float                      m_fFadeFrom = { 1.f };
	_float                      m_fFadeTo = { 0.f };
	_float                      m_fFadeElapsed = { 0.f };
	_float                      m_fFadeDuration = { 0.f };
	function<void()>			m_OnFadeComplete;
	
protected:
	HRESULT						Ready_Components(const UI_IMAGE_DESC* pDesc);
	HRESULT						Bind_ShaderResources();

public:
	static	CUI_Image*			Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject*		Clone(void* pArg) override;
	virtual void				Free() override;
};

NS_END