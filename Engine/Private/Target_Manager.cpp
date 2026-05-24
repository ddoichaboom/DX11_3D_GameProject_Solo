#include "Target_Manager.h"
#include "RenderTarget.h"

CTarget_Manager::CTarget_Manager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : m_pDevice{ pDevice }
    , m_pContext{ pContext }
{
    Safe_AddRef(m_pDevice);
    Safe_AddRef(m_pContext);
}

HRESULT CTarget_Manager::Add_RenderTarget(const _wstring& strTargetTag,
    _uint iWidth, _uint iHeight, DXGI_FORMAT ePixelFormat, const _float4& vClearColor)
{
    if (nullptr != Find_RenderTarget(strTargetTag))
        return E_FAIL;

    CRenderTarget* pRenderTarget = CRenderTarget::Create(
        m_pDevice, m_pContext, iWidth, iHeight, ePixelFormat, vClearColor);

    if (nullptr == pRenderTarget)
        return E_FAIL;

    m_RenderTargets.emplace(strTargetTag, pRenderTarget);
    return S_OK;
}

HRESULT CTarget_Manager::Add_MRT(const _wstring& strMRTTag, const _wstring& strTargetTag)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return E_FAIL;

    list<CRenderTarget*>* pMRTList = Find_MRT(strMRTTag);

    if (nullptr == pMRTList)
    {
        list<CRenderTarget*> MRTList;
        MRTList.push_back(pRenderTarget);
        m_MRTs.emplace(strMRTTag, MRTList);
    }
    else
    {
        pMRTList->push_back(pRenderTarget);
    }

    Safe_AddRef(pRenderTarget);
    return S_OK;
}

HRESULT CTarget_Manager::Begin_MRT(const _wstring& strMRTTag, ID3D11DepthStencilView* pDSV)
{
    list<CRenderTarget*>* pMRTList = Find_MRT(strMRTTag);
    if (nullptr == pMRTList)
        return E_FAIL;

    // 이전 pass 의 SRV 가 새 RTV 와 충돌하지 않도록 PS slot 0~7 풀기
    ID3D11ShaderResourceView* pNullSRVs[8] = {};
    m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

    // 원본 RTV / DSV / Viewport 저장 (End_MRT 에서 복귀)
    m_pContext->OMGetRenderTargets(1, &m_pOriginalRTV, &m_pOriginalDSV);

    m_iNumOriginalViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_pContext->RSGetViewports(&m_iNumOriginalViewports, m_OriginalViewports);

    ID3D11RenderTargetView* pRTVs[8] = {};
    _uint iNumRTVs = 0;

    CRenderTarget* pFirstRenderTarget = nullptr;

    for (CRenderTarget* pRenderTarget : *pMRTList)
    {
        if (nullptr == pRenderTarget || iNumRTVs >= 8)
            continue;

        if (nullptr == pFirstRenderTarget)
            pFirstRenderTarget = pRenderTarget;

        pRenderTarget->Clear();
        pRTVs[iNumRTVs++] = pRenderTarget->Get_RTV();
    }

    if (0 == iNumRTVs || nullptr == pFirstRenderTarget)
        return E_FAIL;

    // *** 핵심 수정 *** :
    // 자체 DSV 만들지 않고 m_pOriginalDSV 를 그대로 재사용한다.
    // 이렇게 해야 NonBlend(MRT) 가 기록한 World mesh 깊이가
    // 이후 NonLight(BackBuffer, NavMesh 등) 까지 살아남아 depth-test 가 정상 동작한다.
    // pDSV 가 외부에서 전달되면 그것을 우선 사용 (특수 pass 용 오버라이드).
    // DSV 자체 Clear 는 호출하지 않는다 — 매 MRT 마다 clear 하면 직전 pass 의 깊이가 지워진다.
    ID3D11DepthStencilView* pBindDSV = (nullptr != pDSV) ? pDSV : m_pOriginalDSV;

    m_pContext->OMSetRenderTargets(iNumRTVs, pRTVs, pBindDSV);

    // Viewport 는 첫 MRT RT 크기로 설정 (ViewportRT 와 MRT 크기 mismatch 방지)
    D3D11_VIEWPORT Viewport{};
    Viewport.TopLeftX = 0.f;
    Viewport.TopLeftY = 0.f;
    Viewport.Width = static_cast<_float>(pFirstRenderTarget->Get_Width());
    Viewport.Height = static_cast<_float>(pFirstRenderTarget->Get_Height());
    Viewport.MinDepth = 0.f;
    Viewport.MaxDepth = 1.f;

    m_pContext->RSSetViewports(1, &Viewport);

    return S_OK;
}

HRESULT CTarget_Manager::End_MRT()
{
    ID3D11RenderTargetView* pRTVs[1] = { m_pOriginalRTV };
    m_pContext->OMSetRenderTargets(1, pRTVs, m_pOriginalDSV);

    if (0 != m_iNumOriginalViewports)
        m_pContext->RSSetViewports(m_iNumOriginalViewports, m_OriginalViewports);

    ID3D11ShaderResourceView* pNullSRVs[8] = {};
    m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

    if (nullptr != m_pOriginalRTV)
    {
        m_pOriginalRTV->Release();
        m_pOriginalRTV = nullptr;
    }

    if (nullptr != m_pOriginalDSV)
    {
        m_pOriginalDSV->Release();
        m_pOriginalDSV = nullptr;
    }

    m_iNumOriginalViewports = 0;

    return S_OK;
}

HRESULT CTarget_Manager::Begin_ViewportRT(_uint iWidth, _uint iHeight)
{
    if (0 == iWidth || 0 == iHeight)
        return E_FAIL;

    CRenderTarget* pViewportTarget = Find_RenderTarget(TEXT("Target_Viewport"));
    if (nullptr == pViewportTarget)
        return E_FAIL;

    if (FAILED(Ready_ViewportDepthStencil(iWidth, iHeight)))
        return E_FAIL;

    m_pContext->OMGetRenderTargets(1, &m_pViewportOriginalRTV, &m_pViewportOriginalDSV);

    m_iNumViewportOriginalViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_pContext->RSGetViewports(&m_iNumViewportOriginalViewports, m_ViewportOriginalViewports);

    ID3D11ShaderResourceView* pNullSRVs[8] = {};
    m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

    ID3D11RenderTargetView* pRTV = pViewportTarget->Get_RTV();
    m_pContext->OMSetRenderTargets(1, &pRTV, m_pViewportDSV);

    D3D11_VIEWPORT Viewport{};
    Viewport.TopLeftX = 0.f;
    Viewport.TopLeftY = 0.f;
    Viewport.Width = static_cast<_float>(iWidth);
    Viewport.Height = static_cast<_float>(iHeight);
    Viewport.MinDepth = 0.f;
    Viewport.MaxDepth = 1.f;
    m_pContext->RSSetViewports(1, &Viewport);

    pViewportTarget->Clear();
    m_pContext->ClearDepthStencilView(m_pViewportDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    return S_OK;
}

HRESULT CTarget_Manager::End_ViewportRT()
{
    ID3D11ShaderResourceView* pNullSRVs[8] = {};
    m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

    ID3D11RenderTargetView* pRTVs[1] = { m_pViewportOriginalRTV };
    m_pContext->OMSetRenderTargets(1, pRTVs, m_pViewportOriginalDSV);

    if (0 != m_iNumViewportOriginalViewports)
        m_pContext->RSSetViewports(m_iNumViewportOriginalViewports, m_ViewportOriginalViewports);

    Safe_Release(m_pViewportOriginalRTV);
    Safe_Release(m_pViewportOriginalDSV);
    m_iNumViewportOriginalViewports = 0;

    return S_OK;
}

ID3D11ShaderResourceView* CTarget_Manager::Get_RenderTargetSRV(const _wstring& strTargetTag)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return nullptr;

    return pRenderTarget->Get_SRV();
}

HRESULT CTarget_Manager::Bind_ShaderResource(const _wstring& strTargetTag, CShader* pShader, const _char* pConstantName)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return E_FAIL;

    return pRenderTarget->Bind_ShaderResource(pShader, pConstantName);
}

HRESULT CTarget_Manager::Resize_RenderTargets(_uint iWidth, _uint iHeight)
{
    ID3D11ShaderResourceView* pNullSRVs[8] = {};
    m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

    if (0 == iWidth || 0 == iHeight)
        return E_FAIL;

    for (auto& Pair : m_RenderTargets)
    {
        if (nullptr != Pair.second)
        {
            if (FAILED(Pair.second->Resize(iWidth, iHeight)))
                return E_FAIL;
        }
    }

    return S_OK;
}

#ifdef _DEBUG

HRESULT CTarget_Manager::Ready_Debug(const _wstring& strTargetTag, _float fX, _float fY, _float fSizeX, _float fSizeY, _float fCanvasWidth, _float fCanvasHeight)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return E_FAIL;

    return pRenderTarget->Ready_Debug(fX, fY, fSizeX, fSizeY, fCanvasWidth, fCanvasHeight);
}

HRESULT CTarget_Manager::Render_Debug(const _wstring& strMRTTag, CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
    list<CRenderTarget*>* pMRTList = Find_MRT(strMRTTag);
    if (nullptr == pMRTList)
        return E_FAIL;

    for (CRenderTarget* pRenderTarget : *pMRTList)
    {
        if (nullptr != pRenderTarget)
        {
            if (FAILED(pRenderTarget->Render_Debug(pShader, pVIBuffer)))
                return E_FAIL;
        }
    }

    return S_OK;
}

#endif

CRenderTarget* CTarget_Manager::Find_RenderTarget(const _wstring& strTargetTag)
{
    auto iter = m_RenderTargets.find(strTargetTag);
    if (iter == m_RenderTargets.end())
        return nullptr;

    return iter->second;
}

list<CRenderTarget*>* CTarget_Manager::Find_MRT(const _wstring& strMRTTag)
{
    auto iter = m_MRTs.find(strMRTTag);
    if (iter == m_MRTs.end())
        return nullptr;

    return &iter->second;
}

HRESULT CTarget_Manager::Ready_ViewportDepthStencil(_uint iWidth, _uint iHeight)
{
    if (iWidth == m_iViewportDSVWidth && iHeight == m_iViewportDSVHeight && nullptr != m_pViewportDSV)
        return S_OK;

    Safe_Release(m_pViewportDSV);
    Safe_Release(m_pViewportDSTexture);

    D3D11_TEXTURE2D_DESC Desc{};
    Desc.Width = iWidth;
    Desc.Height = iHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (FAILED(m_pDevice->CreateTexture2D(&Desc, nullptr, &m_pViewportDSTexture)))
        return E_FAIL;

    if (FAILED(m_pDevice->CreateDepthStencilView(m_pViewportDSTexture, nullptr, &m_pViewportDSV)))
        return E_FAIL;

    m_iViewportDSVWidth = iWidth;
    m_iViewportDSVHeight = iHeight;

    return S_OK;
}

CTarget_Manager* CTarget_Manager::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CTarget_Manager* pInstance = new CTarget_Manager(pDevice, pContext);

    if (nullptr == pInstance)
        return nullptr;

    return pInstance;
}

void CTarget_Manager::Free()
{
    __super::Free();

    for (auto& Pair : m_MRTs)
    {
        for (CRenderTarget* pRenderTarget : Pair.second)
            Safe_Release(pRenderTarget);

        Pair.second.clear();
    }
    m_MRTs.clear();

    for (auto& Pair : m_RenderTargets)
        Safe_Release(Pair.second);

    m_RenderTargets.clear();

    Safe_Release(m_pViewportDSV);
    Safe_Release(m_pViewportDSTexture);

    Safe_Release(m_pContext);
    Safe_Release(m_pDevice);
}