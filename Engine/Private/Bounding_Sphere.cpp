#include "Bounding_Sphere.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "DebugDraw.h"

CBounding_Sphere::CBounding_Sphere(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CBounding{ pDevice, pContext }
{
}

void CBounding_Sphere::Set_Radius(_float fRadius)
{
    if (nullptr != m_pOriginalDesc)
        m_pOriginalDesc->Radius = fRadius;

    if (nullptr != m_pDesc)
        m_pDesc->Radius = fRadius;
}

HRESULT CBounding_Sphere::Initialize(const CBounding::BOUNDING_DESC* pBoundingDesc)
{
    auto pDesc = static_cast<const BOUNDING_SPHERE_DESC*>(pBoundingDesc);

    m_pOriginalDesc = new BoundingSphere(pDesc->vCenter, pDesc->fRadius);
    m_pDesc = new BoundingSphere(*m_pOriginalDesc);

    return S_OK;
}

void CBounding_Sphere::Update(_fmatrix WorldMatrix)
{
    // Sphere 는 회전 무관. 스케일(가장 큰 축)과 평행이동만 영향.
    m_pOriginalDesc->Transform(*m_pDesc, WorldMatrix);
}

_bool CBounding_Sphere::Intersect(COLLIDER eTargetType, CBounding* pTarget)
{
    m_bIsColl = false;

    switch (eTargetType)
    {
    case COLLIDER::AABB:
        m_bIsColl = m_pDesc->Intersects(*dynamic_cast<CBounding_AABB*>(pTarget)->Get_Desc());
        break;
    case COLLIDER::OBB:
        m_bIsColl = m_pDesc->Intersects(*dynamic_cast<CBounding_OBB*>(pTarget)->Get_Desc());
        break;
    case COLLIDER::SPHERE:
        m_bIsColl = m_pDesc->Intersects(*dynamic_cast<CBounding_Sphere*>(pTarget)->Get_Desc());
        break;
    default:
        break;
    }

    return m_bIsColl;
}

#ifdef _DEBUG
HRESULT CBounding_Sphere::Render(PrimitiveBatch<VertexPositionColor>* pBatch)
{
    const XMVECTOR vColor = m_bIsColl
        ? XMVectorSet(1.f, 0.f, 0.f, 1.f)
        : XMVectorSet(0.f, 1.f, 0.f, 1.f);

    DX::Draw(pBatch, *m_pDesc, vColor);
    return S_OK;
}
#endif

CBounding_Sphere* CBounding_Sphere::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const CBounding::BOUNDING_DESC* pDesc)
{
    CBounding_Sphere* pInstance = new CBounding_Sphere(pDevice, pContext);

    if (FAILED(pInstance->Initialize(pDesc)))
    {
        MSG_BOX("Failed to Created : CBounding_Sphere");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CBounding_Sphere::Free()
{
    __super::Free();

    Safe_Delete(m_pDesc);
    Safe_Delete(m_pOriginalDesc);
}