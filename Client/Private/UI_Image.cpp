#include "UI_Image.h"
#include "GameInstance.h"
#include "Texture.h"

CUI_Image::CUI_Image(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CUIObject{ pDevice, pContext }
{
}

CUI_Image::CUI_Image(const CUI_Image& Prototype)
	: CUIObject{ Prototype }
	, m_pShaderCom{ Prototype.m_pShaderCom }
	, m_pVIBufferCom{ Prototype.m_pVIBufferCom }
	, m_pTextureCom{ Prototype.m_pTextureCom }
{
}

void CUI_Image::Set_GaugeRatio(_float fRatio)
{
	if (fRatio < 0.f)
		fRatio = 0.f;
	if (fRatio > 1.f)
		fRatio = 1.f;

	m_fGaugeProgress = fRatio;
}

HRESULT CUI_Image::Set_TextureByProto(_uint iLevel, const _tchar* pProtoTag)
{
	if (nullptr == pProtoTag || L'\0' == pProtoTag[0] || nullptr == m_pGameInstance)
		return E_FAIL;

	CBase* pClone = m_pGameInstance->Clone_Prototype(PROTOTYPE::COMPONENT, iLevel, pProtoTag);
	if (nullptr == pClone)
		return E_FAIL;

	Safe_Release(m_pTextureCom);
	m_pTextureCom = static_cast<CTexture*>(pClone);
	return S_OK;
}

void CUI_Image::Set_Center(_float fCenterX, _float fCenterY)
{
	m_fCenterX = fCenterX;
	m_fCenterY = fCenterY;
	Update_UIState();
}

void CUI_Image::Set_Size(_float fSizeX, _float fSizeY)
{
	m_fSizeX = fSizeX;
	m_fSizeY = fSizeY;
	Update_UIState();
}

void CUI_Image::Start_Fade(_float fTargetAlpha, _float fDuration, function<void()> OnComplete)
{
	m_fFadeFrom = m_fAlpha;
	m_fFadeTo = fTargetAlpha;
	m_fFadeDuration = (fDuration > 0.f) ? fDuration : 0.001f;
	m_fFadeElapsed = 0.f;
	m_bFading = true;
	m_OnFadeComplete = move(OnComplete);
}

void CUI_Image::Stop_Fade()
{
	m_bFading = false;
	m_OnFadeComplete = nullptr;
}

HRESULT CUI_Image::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CUI_Image::Initialize(void* pArg)
{
	auto pDesc = static_cast<UI_IMAGE_DESC*>(pArg);

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_vColor		= pDesc->vColor;
	m_eSweepMode	= pDesc->eSweepMode;

	if (FAILED(Ready_Components(pDesc)))
		return E_FAIL;

	return S_OK;
}

void CUI_Image::Update(_float fTimeDelta)
{
	if (!m_bFading)
		return;

	m_fFadeElapsed += fTimeDelta;
	_float fT = (m_fFadeDuration > 0.f) ? (m_fFadeElapsed / m_fFadeDuration) : 1.f;

	if (fT >= 1.f)
	{
		fT = 1.f;
		m_fAlpha = m_fFadeTo;
		m_bFading = false;

		auto cb = move(m_OnFadeComplete);
		m_OnFadeComplete = nullptr;
		if (cb)
			cb();
		return;
	}

	m_fAlpha = m_fFadeFrom + (m_fFadeTo - m_fFadeFrom) * fT;
}

void CUI_Image::Late_Update(_float fTimeDelta)
{
	if (!m_bVisible)
		return;

	m_pGameInstance->Add_RenderGroup(RENDERID::UI, this);
}

HRESULT CUI_Image::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	_uint iPassIndex = 1;   
	if (m_eSweepMode == UI_SWEEP_MODE::POSITION)
		iPassIndex = 3;     
	else if (m_eSweepMode == UI_SWEEP_MODE::UV)
		iPassIndex = 4;
	else if (m_eSweepMode == UI_SWEEP_MODE::BOX)
		iPassIndex = 5;
	else if (m_eSweepMode == UI_SWEEP_MODE::FILL)
		iPassIndex = 6;
	else if (m_eSweepMode == UI_SWEEP_MODE::GAUGE_V)
		iPassIndex = 7;     

	if (FAILED(m_pShaderCom->Begin(iPassIndex)))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT CUI_Image::Ready_Components(const UI_IMAGE_DESC* pDesc)
{
	if (FAILED(__super::Add_Component(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_Shader_VtxTex"),
		TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_VIBuffer_Rect"),
		TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
		return E_FAIL;

	// Pattern A: 프로토타입 풀에서 Clone 우선
	const _bool bUseProto = (nullptr != pDesc->pTextureProtoTag) && (L'\0' != pDesc->pTextureProtoTag[0]);

	if (bUseProto)
	{
		if (FAILED(__super::Add_Component(pDesc->iTextureProtoLevel,
			pDesc->pTextureProtoTag,
			TEXT("Com_Texture"), reinterpret_cast<CComponent**>(&m_pTextureCom))))
			return E_FAIL;
	}
	else
	{
		// Pattern B: 경로 기반 직접 로드 (Editor 미리보기/일회성 용)
		m_pTextureCom = Engine::CTexture::Create(m_pDevice, m_pContext, pDesc->pTexturePath, 1);
		if (nullptr == m_pTextureCom)
			return E_FAIL;
	}

	return S_OK;
}

HRESULT CUI_Image::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(m_pShaderCom, "g_ViewMatrix", D3DTS::VIEW)))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(m_pShaderCom, "g_ProjMatrix", D3DTS::PROJ)))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &m_fAlpha, sizeof(_float))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fGaugeProgress", &m_fGaugeProgress, sizeof(_float))))
		return E_FAIL;

	(void)m_bGaugeVertical;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_vColor", &m_vColor, sizeof(_float4))))
		return E_FAIL;

	if (m_eSweepMode != UI_SWEEP_MODE::NONE)
	{
		if (FAILED(m_pShaderCom->Bind_RawValue("g_vSweepTint", &m_vColor, sizeof(_float4))))
			return E_FAIL;
		if (FAILED(m_pShaderCom->Bind_RawValue("g_vUVOffset", &m_vUVOffset, sizeof(_float4))))
			return E_FAIL;
	}

	return S_OK;
}

CUI_Image* CUI_Image::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CUI_Image* pInstance = new CUI_Image(pDevice, pContext);
	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CUI_Image");
		Safe_Release(pInstance);
	}
	return pInstance;
}

CGameObject* CUI_Image::Clone(void* pArg)
{
	CUI_Image* pInstance = new CUI_Image(*this);
	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CUI_Image");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CUI_Image::Free()
{
	__super::Free();

	Safe_Release(m_pShaderCom);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pVIBufferCom);
}
