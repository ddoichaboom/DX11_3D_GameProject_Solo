#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CFrustum final : public CBase
{
private:
    CFrustum();
    virtual ~CFrustum() = default;

public:
    HRESULT             Initialize();
    void                Update(const _float4x4* pViewInv, const _float4x4* pProjInv);

    void                Transform_ToLocalSpace(_fmatrix WorldMatrix);

    _bool               Is_InWorldSpace(_fvector vWorldPos, _float fRange = 0.f) const;
    _bool               Is_InLocalSpace(_fvector vLocalPos, _float fRange = 0.f) const;

private:
    void                Make_Planes(const _float4* pPoints, _float4* pPlanes);

private:
    _float4             m_vOriginalPoints[8]    = {};

    _float4             m_vWorldPoints[8]       = {};
    _float4             m_vWorldPlanes[6]       = {};

    _float4             m_vLocalPoints[8]       = {};
    _float4             m_vLocalPlanes[6]       = {};

public:
    static CFrustum*    Create();
    virtual void        Free() override;
};

NS_END

