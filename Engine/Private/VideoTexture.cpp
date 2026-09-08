#include "VideoTexture.h"

CVideoTexture::CVideoTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : m_pDevice{ pDevice }
    , m_pContext{ pContext }
{
    Safe_AddRef(m_pDevice);
    Safe_AddRef(m_pContext);
}

CVideoTexture* CVideoTexture::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
    const _tchar* pVideoPath, _bool bLoop)
{
    CVideoTexture* pInstance = new CVideoTexture(pDevice, pContext);

    if (FAILED(pInstance->Initialize(pVideoPath, bLoop)))
    {
        MSG_BOX("Failed to Created : CVideoTexture");
        Safe_Release(pInstance);
    }
    return pInstance;
}

HRESULT CVideoTexture::Initialize(const _tchar* pVideoPath, _bool bLoop)
{
    if (nullptr == pVideoPath || 0 == pVideoPath[0])
        return E_FAIL;

    // CoInitializeEx 는 S_FALSE(이미 초기화) 도 허용.
    // MFStartup 은 내부 RefCount — Free 의 MFShutdown 과 짝만 맞으면 OK.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (FAILED(MFStartup(MF_VERSION)))
        return E_FAIL;

    m_bLoop = bLoop;

    //if (FAILED(MFCreateSourceReaderFromURL(pVideoPath, nullptr, &m_pSourceReader)))
    //    return E_FAIL;
    IMFAttributes* pAttrs = nullptr;
    if (FAILED(MFCreateAttributes(&pAttrs, 1)))
        return E_FAIL;
    pAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

    HRESULT hrCreate = MFCreateSourceReaderFromURL(pVideoPath, pAttrs, &m_pSourceReader);
    Safe_Release(pAttrs);

    if (FAILED(hrCreate))
        return E_FAIL;

    if (FAILED(Configure_SourceReader()))
        return E_FAIL;

    if (FAILED(Create_DynamicTexture()))
        return E_FAIL;

    if (FAILED(Read_NextSample()))
        return E_FAIL;

    if (m_bHasPendingSample)
    {
        if (FAILED(Upload_PendingSample()))
            return E_FAIL;
    }
    return S_OK;
}

HRESULT CVideoTexture::Configure_SourceReader()
{
    if (FAILED(m_pSourceReader->SetStreamSelection(
        static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE)))
        return E_FAIL;

    if (FAILED(m_pSourceReader->SetStreamSelection(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE)))
        return E_FAIL;

    // 출력 포맷을 RGB32 (BGRA in memory) 로 강제 — D3D11 의 B8G8R8A8_UNORM 와 1:1
    IMFMediaType* pOutType = nullptr;
    if (FAILED(MFCreateMediaType(&pOutType)))
        return E_FAIL;

    HRESULT hr = pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) 
        hr = pOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (SUCCEEDED(hr)) 
        hr = m_pSourceReader->SetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pOutType);
    Safe_Release(pOutType);
    if (FAILED(hr))
        return E_FAIL;

    // 협상된 최종 타입에서 해상도 추출
    IMFMediaType* pCurType = nullptr;
    if (FAILED(m_pSourceReader->GetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &pCurType)))
        return E_FAIL;

    UINT32 w = 0, h = 0;
    MFGetAttributeSize(pCurType, MF_MT_FRAME_SIZE, &w, &h);
    Safe_Release(pCurType);

    m_iVideoWidth = w;
    m_iVideoHeight = h;
    m_iSourceStride = w * 4;   // RGB32 = 4 bytes/pixel

    return S_OK;
}

HRESULT CVideoTexture::Create_DynamicTexture()
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = m_iVideoWidth;
    desc.Height = m_iVideoHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(m_pDevice->CreateTexture2D(&desc, nullptr, &m_pTexture)))
        return E_FAIL;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(m_pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pSRV)))
        return E_FAIL;

    return S_OK;
}

HRESULT CVideoTexture::Read_NextSample()
{
    DWORD       streamIndex = 0;
    DWORD       flags = 0;
    LONGLONG    timestamp = 0;
    IMFSample* pSample = nullptr;

    HRESULT hr = m_pSourceReader->ReadSample(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0,
        &streamIndex, &flags, &timestamp, &pSample);

    if (FAILED(hr))
        return E_FAIL;

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
        if (pSample) pSample->Release();

        if (m_bLoop)
        {
            PROPVARIANT var{};
            var.vt = VT_I8;
            var.hVal.QuadPart = 0;
            m_pSourceReader->SetCurrentPosition(GUID_NULL, var);
            PropVariantClear(&var);
            m_llPlaybackTime = 0;
            return Read_NextSample();
        }
        m_bFinished = true;
        return S_OK;
    }

    if (nullptr == pSample)
        return S_OK;   // 디코더가 아직 준비 안 됨 — 다음 tick 에 재시도

    Release_PendingSample();
    m_pPendingSample = pSample;
    m_llPendingPTS = timestamp;
    m_bHasPendingSample = true;
    return S_OK;
}

HRESULT CVideoTexture::Upload_PendingSample()
{
    if (!m_bHasPendingSample || nullptr == m_pPendingSample)
        return S_OK;

    IMFMediaBuffer* pBuf = nullptr;
    if (FAILED(m_pPendingSample->ConvertToContiguousBuffer(&pBuf)))
    {
        Release_PendingSample();
        return E_FAIL;
    }

    BYTE* pData = nullptr;
    DWORD   maxLen = 0;
    DWORD   curLen = 0;

    if (SUCCEEDED(pBuf->Lock(&pData, &maxLen, &curLen)))
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_pContext->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            for (UINT y = 0; y < m_iVideoHeight; ++y)
            {
                BYTE* dstRow = reinterpret_cast<BYTE*>(mapped.pData) + y * mapped.RowPitch;
                BYTE* srcRow = pData + y * m_iSourceStride;
                memcpy(dstRow, srcRow, m_iSourceStride);

                for (UINT x = 3; x < m_iSourceStride; x += 4)
                    dstRow[x] = 0xFF;
            }
            m_pContext->Unmap(m_pTexture, 0);
        }
        pBuf->Unlock();
    }

    Safe_Release(pBuf);
    Release_PendingSample();
    return S_OK;
}

void CVideoTexture::Release_PendingSample()
{
    if (m_pPendingSample)
    {
        m_pPendingSample->Release();
        m_pPendingSample = nullptr;
    }
    m_bHasPendingSample = false;
}

HRESULT CVideoTexture::Update(_float fTimeDelta)
{
    if (nullptr == m_pSourceReader || m_bFinished)
        return S_OK;

    m_llPlaybackTime += static_cast<LONGLONG>(
        static_cast<double>(fTimeDelta) * m_fPlaybackSpeed * 10000000.0);

    for (_int i = 0; i < 16; ++i)
    {
        if (!m_bHasPendingSample)
        {
            if (FAILED(Read_NextSample()))
                return E_FAIL;
            if (m_bFinished || !m_bHasPendingSample)
                break;
        }

        if (m_llPendingPTS > m_llPlaybackTime)
            break;

        break;
    }

    return S_OK;
}

HRESULT CVideoTexture::Upload_ReadyFrame()
{
    if (nullptr == m_pSourceReader || m_bFinished)
        return S_OK;

    if (m_bHasPendingSample && m_llPendingPTS <= m_llPlaybackTime)
        return Upload_PendingSample();

    return S_OK;
}

void CVideoTexture::Reset()
{
    if (nullptr == m_pSourceReader)
        return;

    PROPVARIANT var{};
    var.vt = VT_I8;
    var.hVal.QuadPart = 0;
    m_pSourceReader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    m_llPlaybackTime = 0;
    m_bFinished = false;
    Release_PendingSample();
}

void CVideoTexture::Free()
{
    __super::Free();

    Release_PendingSample();

    Safe_Release(m_pSRV);
    Safe_Release(m_pTexture);

    if (m_pSourceReader)
    {
        m_pSourceReader->Release();
        m_pSourceReader = nullptr;
    }

    Safe_Release(m_pContext);
    Safe_Release(m_pDevice);

    MFShutdown();  // Initialize 의 MFStartup 과 짝
}