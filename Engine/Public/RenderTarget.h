#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CShader;

class ENGINE_DLL CRenderTarget final : public CBase
{
private:
    CRenderTarget(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual ~CRenderTarget() = default;

public:
    ID3D11RenderTargetView*     Get_RTV() const { return m_pRTV; }
    ID3D11ShaderResourceView*   Get_SRV() const { return m_pSRV; }

public:
    HRESULT                     Initialize(_uint iWidth, _uint iHeight, DXGI_FORMAT ePixelFormat, const _float4& vClearColor);
    void                        Clear();
    HRESULT                     Bind_ShaderResource(CShader* pShader, const _char* pConstantName);

#ifdef _DEBUG
public:
    HRESULT                     Ready_Debug(_float fX, _float fY, _float fSizeX, _float fSizeY);
    HRESULT                     Render_Debug(class CShader* pShader, class CVIBuffer_Rect* pVIBuffer);
#endif

private:
    ID3D11Device*               m_pDevice = { nullptr };
    ID3D11DeviceContext*        m_pContext = { nullptr };

    ID3D11Texture2D*            m_pTexture2D = { nullptr };
    ID3D11RenderTargetView*     m_pRTV = { nullptr };
    ID3D11ShaderResourceView*   m_pSRV = { nullptr };

private:
    _float4                     m_vClearColor = {};

#ifdef _DEBUG
    _float4x4                   m_WorldMatrix = {};
#endif

public:
    static CRenderTarget*       Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                                        _uint iWidth, _uint iHeight,
                                        DXGI_FORMAT ePixelFormat, const _float4& vClearColor);
    virtual void                Free() override;
};

NS_END