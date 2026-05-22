#include "RenderTarget.h"
#include "Shader.h"
#include "GameInstance.h"

CRenderTarget::CRenderTarget(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : m_pDevice{ pDevice }
    , m_pContext{ pContext }
{
    Safe_AddRef(m_pDevice);
    Safe_AddRef(m_pContext);
}

HRESULT CRenderTarget::Initialize(_uint iWidth, _uint iHeight, DXGI_FORMAT ePixelFormat, const _float4& vClearColor)
{
    m_vClearColor = vClearColor;

    D3D11_TEXTURE2D_DESC TextureDesc{};
    TextureDesc.Width = iWidth;
    TextureDesc.Height = iHeight;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = ePixelFormat;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;

    if (FAILED(m_pDevice->CreateTexture2D(&TextureDesc, nullptr, &m_pTexture2D)))
        return E_FAIL;

    if (FAILED(m_pDevice->CreateRenderTargetView(m_pTexture2D, nullptr, &m_pRTV)))
        return E_FAIL;

    if (FAILED(m_pDevice->CreateShaderResourceView(m_pTexture2D, nullptr, &m_pSRV)))
        return E_FAIL;

    return S_OK;
}

void CRenderTarget::Clear()
{
    if (nullptr == m_pContext || nullptr == m_pRTV)
        return;

    m_pContext->ClearRenderTargetView(m_pRTV, reinterpret_cast<const _float*>(&m_vClearColor));
}

HRESULT CRenderTarget::Bind_ShaderResource(CShader* pShader, const _char* pConstantName)
{
    if (nullptr == pShader || nullptr == m_pSRV)
        return E_FAIL;

    return pShader->Bind_SRV(pConstantName, m_pSRV);
}

#ifdef _DEBUG

HRESULT CRenderTarget::Ready_Debug(_float fX, _float fY, _float fSizeX, _float fSizeY)
{
    CGameInstance* pGameInstance = CGameInstance::GetInstance();

    const _float fWinSizeX = static_cast<_float>(pGameInstance->Get_WinSizeX());
    const _float fWinSizeY = static_cast<_float>(pGameInstance->Get_WinSizeY());

    XMStoreFloat4x4(
        &m_WorldMatrix,
        XMMatrixScaling(fSizeX, fSizeY, 1.f) *
        XMMatrixTranslation(
            fX - fWinSizeX * 0.5f,
            -fY + fWinSizeY * 0.5f,
            0.f));

    return S_OK;
}

HRESULT CRenderTarget::Render_Debug(CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
    if (nullptr == pShader || nullptr == pVIBuffer)
        return E_FAIL;

    if (FAILED(pShader->Bind_Matrix("g_WorldMatrix", &m_WorldMatrix)))
        return E_FAIL;

    if (FAILED(pShader->Bind_SRV("g_Texture", m_pSRV)))
        return E_FAIL;

    if (FAILED(pShader->Begin(0)))
        return E_FAIL;

    if (FAILED(pVIBuffer->Render()))
        return E_FAIL;

    ID3D11ShaderResourceView* pNullSRV[1] = { nullptr };
    m_pContext->PSSetShaderResources(0, 1, pNullSRV);

    return S_OK;
}

#endif

CRenderTarget* CRenderTarget::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
    _uint iWidth, _uint iHeight, DXGI_FORMAT ePixelFormat, const _float4& vClearColor)
{
    CRenderTarget* pInstance = new CRenderTarget(pDevice, pContext);

    if (FAILED(pInstance->Initialize(iWidth, iHeight, ePixelFormat, vClearColor)))
    {
        MSG_BOX("Failed to Created : CRenderTarget");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CRenderTarget::Free()
{
    __super::Free();

    Safe_Release(m_pSRV);
    Safe_Release(m_pRTV);
    Safe_Release(m_pTexture2D);

    Safe_Release(m_pContext);
    Safe_Release(m_pDevice);
}