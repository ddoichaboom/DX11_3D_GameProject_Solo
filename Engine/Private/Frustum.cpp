#include "Frustum.h"

CFrustum::CFrustum()
{
}

HRESULT CFrustum::Initialize()
{
    m_vOriginalPoints[0] = _float4(-1.f, 1.f, 0.f, 1.f);
    m_vOriginalPoints[1] = _float4(1.f, 1.f, 0.f, 1.f);
    m_vOriginalPoints[2] = _float4(1.f, -1.f, 0.f, 1.f);
    m_vOriginalPoints[3] = _float4(-1.f, -1.f, 0.f, 1.f);

    m_vOriginalPoints[4] = _float4(-1.f, 1.f, 1.f, 1.f);
    m_vOriginalPoints[5] = _float4(1.f, 1.f, 1.f, 1.f);
    m_vOriginalPoints[6] = _float4(1.f, -1.f, 1.f, 1.f);
    m_vOriginalPoints[7] = _float4(-1.f, -1.f, 1.f, 1.f);

    return S_OK;
}

void CFrustum::Update(const _float4x4* pViewInv, const _float4x4* pProjInv)
{
    if (nullptr == pViewInv || nullptr == pProjInv)
        return;

    const _matrix ProjInv = XMLoadFloat4x4(pProjInv);
    const _matrix ViewInv = XMLoadFloat4x4(pViewInv);

    for (_uint i = 0; i < 8; ++i)
    {
        _vector vPoint = XMLoadFloat4(&m_vOriginalPoints[i]);
        vPoint = XMVector3TransformCoord(vPoint, ProjInv);
        vPoint = XMVector3TransformCoord(vPoint, ViewInv);

        XMStoreFloat4(&m_vWorldPoints[i], vPoint);
    }

    Make_Planes(m_vWorldPoints, m_vWorldPlanes);
}

void CFrustum::Transform_ToLocalSpace(_fmatrix WorldMatrix)
{
    _matrix WorldInv = XMMatrixInverse(nullptr, WorldMatrix);

    for (_uint i = 0; i < 8; ++i)
    {
        XMStoreFloat4(&m_vLocalPoints[i],
            XMVector3TransformCoord(XMLoadFloat4(&m_vWorldPoints[i]), WorldInv));
    }

    Make_Planes(m_vLocalPoints, m_vLocalPlanes);
}

_bool CFrustum::Is_InWorldSpace(_fvector vWorldPos, _float fRange) const
{
    for (_uint i = 0; i < 6; ++i)
    {
        if (fRange <= XMVectorGetX(XMPlaneDotCoord(XMLoadFloat4(&m_vWorldPlanes[i]), vWorldPos)))
            return false;
    }

    return true;
}

_bool CFrustum::Is_InLocalSpace(_fvector vLocalPos, _float fRange) const
{
    for (_uint i = 0; i < 6; ++i)
    {
        if (fRange <= XMVectorGetX(XMPlaneDotCoord(XMLoadFloat4(&m_vLocalPlanes[i]), vLocalPos)))
            return false;
    }

    return true;
}

void CFrustum::Make_Planes(const _float4* pPoints, _float4* pPlanes)
{
    XMStoreFloat4(&pPlanes[0], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[1]),
        XMLoadFloat4(&pPoints[5]),
        XMLoadFloat4(&pPoints[6])));

    XMStoreFloat4(&pPlanes[1], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[4]),
        XMLoadFloat4(&pPoints[0]),
        XMLoadFloat4(&pPoints[3])));

    XMStoreFloat4(&pPlanes[2], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[4]),
        XMLoadFloat4(&pPoints[5]),
        XMLoadFloat4(&pPoints[1])));

    XMStoreFloat4(&pPlanes[3], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[3]),
        XMLoadFloat4(&pPoints[2]),
        XMLoadFloat4(&pPoints[6])));

    XMStoreFloat4(&pPlanes[4], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[5]),
        XMLoadFloat4(&pPoints[4]),
        XMLoadFloat4(&pPoints[7])));

    XMStoreFloat4(&pPlanes[5], XMPlaneFromPoints(
        XMLoadFloat4(&pPoints[0]),
        XMLoadFloat4(&pPoints[1]),
        XMLoadFloat4(&pPoints[2])));
}

CFrustum* CFrustum::Create()
{
    CFrustum* pInstance = new CFrustum();

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : CFrustum");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CFrustum::Free()
{
    __super::Free();
}