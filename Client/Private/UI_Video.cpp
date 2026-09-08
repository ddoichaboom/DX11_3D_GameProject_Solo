#include "UI_Video.h"
#include "GameInstance.h"
#include "Shader.h"
#include "VIBuffer.h"
#include "Transform.h"
#include "VideoTexture.h"

CUI_Video::CUI_Video(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CUIObject{ pDevice, pContext }
{
}

CUI_Video::CUI_Video(const CUI_Video& Prototype)
    : CUIObject{ Prototype }
    , m_pShaderCom{ Prototype.m_pShaderCom }
    , m_pVIBufferCom{ Prototype.m_pVIBufferCom }
{
    // m_pVideoTexture 는 인스턴스마다 별도 생성 (Initialize 에서)
}

HRESULT CUI_Video::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CUI_Video::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    auto pDesc = static_cast<UI_VIDEO_DESC*>(pArg);

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components(pDesc->pVideoPath, pDesc->bLoop, pDesc->fPlaybackSpeed)))
        return E_FAIL;


    m_bPlaying = pDesc->bVisible;
    return S_OK;
}

void CUI_Video::Play()
{
    m_bPlaying = true;
    Set_Visible(true);
}

void CUI_Video::Pause()
{
    m_bPlaying = false;
}

void CUI_Video::Stop()
{
    m_bPlaying = false;
    Set_Visible(false);

    if (nullptr != m_pVideoTexture)
        m_pVideoTexture->Reset();
}

void CUI_Video::Reset()
{
    if (nullptr != m_pVideoTexture)
        m_pVideoTexture->Reset();
}

_bool CUI_Video::Is_Finished() const
{
    return (nullptr != m_pVideoTexture) ? m_pVideoTexture->Is_Finished() : true;
}

void CUI_Video::Update(_float fTimeDelta)
{
    if (false == m_bVisible || false == m_bPlaying)
        return;

    if (m_pVideoTexture)
        m_pVideoTexture->Update(fTimeDelta);
}
void CUI_Video::Late_Update(_float fTimeDelta)
{
    if (!m_bVisible)
        return;

    m_pGameInstance->Add_RenderGroup(RENDERID::UI, this);
}

HRESULT CUI_Video::Render()
{
    if (nullptr != m_pVideoTexture)
    {
        if (FAILED(m_pVideoTexture->Upload_ReadyFrame()))
            return E_FAIL;
    }

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Begin(1)))
        return E_FAIL;

    if (FAILED(m_pVIBufferCom->Bind_Resources()))
        return E_FAIL;

    if (FAILED(m_pVIBufferCom->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT CUI_Video::Ready_Components(const _tchar* pVideoPath, _bool bLoop, _float fSpeed)
{
    if (FAILED(__super::Add_Component(ETOUI(LEVEL::STATIC),
        TEXT("Prototype_Component_Shader_VtxTex"),
        TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOUI(LEVEL::STATIC),
        TEXT("Prototype_Component_VIBuffer_Rect"),
        TEXT("Com_VIBuffer"), reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
        return E_FAIL;

    m_pVideoTexture = CVideoTexture::Create(m_pDevice, m_pContext, pVideoPath, bLoop);
    if (nullptr == m_pVideoTexture)
        return E_FAIL;

    m_pVideoTexture->Set_Speed(fSpeed);
    return S_OK;
}

HRESULT CUI_Video::Bind_ShaderResources()
{
    if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
        return E_FAIL;

    if (FAILED(__super::Bind_ShaderResource(m_pShaderCom, "g_ViewMatrix", D3DTS::VIEW)))
        return E_FAIL;

    if (FAILED(__super::Bind_ShaderResource(m_pShaderCom, "g_ProjMatrix", D3DTS::PROJ)))
        return E_FAIL;

    ID3D11ShaderResourceView* pSRV = m_pVideoTexture->Get_SRV();
    if (nullptr == pSRV)
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_SRV("g_Texture", pSRV)))
        return E_FAIL;

    const _float fOne = 1.f;
    if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &fOne, sizeof(_float))))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_RawValue("g_fGaugeProgress", &fOne, sizeof(_float))))
        return E_FAIL;



    return S_OK;
}

CUI_Video* CUI_Video::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CUI_Video* pInstance = new CUI_Video(pDevice, pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CUI_Video");
        Safe_Release(pInstance);
    }
    return pInstance;
}

CGameObject* CUI_Video::Clone(void* pArg)
{
    CUI_Video* pInstance = new CUI_Video(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CUI_Video");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CUI_Video::Free()
{
    __super::Free();

    Safe_Release(m_pVideoTexture);
    Safe_Release(m_pVIBufferCom);
    Safe_Release(m_pShaderCom);
}
