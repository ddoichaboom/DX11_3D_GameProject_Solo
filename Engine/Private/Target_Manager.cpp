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

    m_pContext->OMGetRenderTargets(1, &m_pOriginalRTV, &m_pOriginalDSV);

    ID3D11RenderTargetView* pRTVs[8] = {};
    _uint iNumRTVs = 0;

    for (CRenderTarget* pRenderTarget : *pMRTList)
    {
        if (nullptr == pRenderTarget || iNumRTVs >= 8)
            continue;

        pRenderTarget->Clear();
        pRTVs[iNumRTVs++] = pRenderTarget->Get_RTV();
    }

    if (0 == iNumRTVs)
        return E_FAIL;

    m_pContext->OMSetRenderTargets(
        iNumRTVs,
        pRTVs,
        nullptr != pDSV ? pDSV : m_pOriginalDSV);

    return S_OK;
}

HRESULT CTarget_Manager::End_MRT()
{
    ID3D11RenderTargetView* pRTVs[1] = { m_pOriginalRTV };
    m_pContext->OMSetRenderTargets(1, pRTVs, m_pOriginalDSV);

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

    return S_OK;
}

HRESULT CTarget_Manager::Bind_ShaderResource(const _wstring& strTargetTag, CShader* pShader, const _char* pConstantName)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return E_FAIL;

    return pRenderTarget->Bind_ShaderResource(pShader, pConstantName);
}

#ifdef _DEBUG

HRESULT CTarget_Manager::Ready_Debug(const _wstring& strTargetTag, _float fX, _float fY, _float fSizeX, _float fSizeY)
{
    CRenderTarget* pRenderTarget = Find_RenderTarget(strTargetTag);
    if (nullptr == pRenderTarget)
        return E_FAIL;

    return pRenderTarget->Ready_Debug(fX, fY, fSizeX, fSizeY);
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

    Safe_Release(m_pContext);
    Safe_Release(m_pDevice);
}