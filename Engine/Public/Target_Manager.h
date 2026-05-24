#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CRenderTarget;
class CShader;

class ENGINE_DLL CTarget_Manager final : public CBase
{
private:
    CTarget_Manager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual ~CTarget_Manager() = default;

public:
    HRESULT                                     Add_RenderTarget(const _wstring& strTargetTag,
                                                                    _uint iWidth, _uint iHeight,
                                                                    DXGI_FORMAT ePixelFormat, const _float4& vClearColor);

    HRESULT                                     Add_MRT(const _wstring& strMRTTag, const _wstring& strTargetTag);
    HRESULT                                     Begin_MRT(const _wstring& strMRTTag, ID3D11DepthStencilView* pDSV = nullptr);
    HRESULT                                     End_MRT();

    HRESULT                                     Begin_ViewportRT(_uint iWidth, _uint iHeight);
    HRESULT                                     End_ViewportRT();
    ID3D11ShaderResourceView*                   Get_RenderTargetSRV(const _wstring& strTargetTag);

    HRESULT                                     Bind_ShaderResource(const _wstring& strTargetTag, CShader* pShader, const _char* pConstantName);

    HRESULT                                     Resize_RenderTargets(_uint iWidth, _uint iHeight);

#ifdef _DEBUG
public:
    HRESULT                                     Ready_Debug(const _wstring& strTargetTag,
                                                            _float fX, _float fY,
                                                            _float fSizeX, _float fSizeY,
                                                            _float fCanvasWidth, _float fCanvasHeight);
    HRESULT                                     Render_Debug(const _wstring& strMRTTag, class CShader* pShader, class CVIBuffer_Rect* pVIBuffer);
#endif


private:
    CRenderTarget*                              Find_RenderTarget(const _wstring& strTargetTag);
    list<CRenderTarget*>*                       Find_MRT(const _wstring& strMRTTag);

    HRESULT                                     Ready_ViewportDepthStencil(_uint iWidth, _uint iHeight);

private:
    ID3D11Device*                               m_pDevice = { nullptr };
    ID3D11DeviceContext*                        m_pContext = { nullptr };

    ID3D11RenderTargetView*                     m_pOriginalRTV = { nullptr };
    ID3D11DepthStencilView*                     m_pOriginalDSV = { nullptr };

    D3D11_VIEWPORT                              m_OriginalViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    _uint                                       m_iNumOriginalViewports = {};

    ID3D11DepthStencilView*                     m_pViewportDSV = { nullptr };
    ID3D11Texture2D*                            m_pViewportDSTexture = { nullptr };
    _uint                                       m_iViewportDSVWidth = {};
    _uint                                       m_iViewportDSVHeight = {};

    ID3D11RenderTargetView*                     m_pViewportOriginalRTV = { nullptr };
    ID3D11DepthStencilView*                     m_pViewportOriginalDSV = { nullptr };
    D3D11_VIEWPORT                              m_ViewportOriginalViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    _uint                                       m_iNumViewportOriginalViewports = {};



private:
    map<const _wstring, CRenderTarget*>         m_RenderTargets;
    map<const _wstring, list<CRenderTarget*>>   m_MRTs;

public:
    static CTarget_Manager*                     Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual void                                Free() override;
};

NS_END

