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

    HRESULT                                     Bind_ShaderResource(const _wstring& strTargetTag, CShader* pShader, const _char* pConstantName);

#ifdef _DEBUG
public:
    HRESULT                                     Ready_Debug(const _wstring& strTargetTag, _float fX, _float fY, _float fSizeX, _float fSizeY);
    HRESULT                                     Render_Debug(const _wstring& strMRTTag, class CShader* pShader, class CVIBuffer_Rect* pVIBuffer);
#endif

private:
    CRenderTarget*                              Find_RenderTarget(const _wstring& strTargetTag);
    list<CRenderTarget*>*                       Find_MRT(const _wstring& strMRTTag);

private:
    ID3D11Device*                               m_pDevice = { nullptr };
    ID3D11DeviceContext*                        m_pContext = { nullptr };

    ID3D11RenderTargetView*                     m_pOriginalRTV = { nullptr };
    ID3D11DepthStencilView*                     m_pOriginalDSV = { nullptr };

    map<const _wstring, CRenderTarget*>         m_RenderTargets;
    map<const _wstring, list<CRenderTarget*>>   m_MRTs;

public:
    static CTarget_Manager*                     Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual void                                Free() override;
};

NS_END

