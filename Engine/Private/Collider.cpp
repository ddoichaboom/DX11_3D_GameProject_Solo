#include "Collider.h"
#include "GameInstance.h"
#include "Bounding.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"

CCollider::CCollider(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CComponent{ pDevice, pContext }
{
}

CCollider::CCollider(const CCollider& Prototype)
    : CComponent{ Prototype }
{
}

HRESULT CCollider::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CCollider::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    auto pDesc = static_cast<COLLIDER_DESC*>(pArg);

    m_eBoundingType = pDesc->eBoundingType;
    m_eGroup = pDesc->eGroup;
    m_pOwner = pDesc->pOwner;

    switch (m_eBoundingType)
    {
    case COLLIDER::AABB:
    {
        CBounding_AABB::BOUNDING_AABB_DESC Desc{};
        Desc.vCenter = pDesc->vCenter;
        Desc.vSize = pDesc->vSize;
        m_pBounding = CBounding_AABB::Create(m_pDevice, m_pContext, &Desc);
    }
    break;
    case COLLIDER::OBB:
    {
        CBounding_OBB::BOUNDING_OBB_DESC Desc{};
        Desc.vCenter = pDesc->vCenter;
        Desc.vSize = pDesc->vSize;
        Desc.vRadians = pDesc->vRadians;
        m_pBounding = CBounding_OBB::Create(m_pDevice, m_pContext, &Desc);
    }
    break;
    case COLLIDER::SPHERE:
    {
        CBounding_Sphere::BOUNDING_SPHERE_DESC Desc{};
        Desc.vCenter = pDesc->vCenter;
        Desc.fRadius = pDesc->fRadius;
        m_pBounding = CBounding_Sphere::Create(m_pDevice, m_pContext, &Desc);
    }
    break;
    default:
        return E_FAIL;
    }

    if (nullptr == m_pBounding)
        return E_FAIL;

    return S_OK;
}

void CCollider::Update(_fmatrix WorldMatrix)
{
    if (m_pBounding)
    {
        m_pBounding->Set_Coll(false);       // 매 프레임 색 리셋
        m_pBounding->Update(WorldMatrix);
    }
}

void CCollider::Register()
{
    if (m_eGroup >= COLLISION_GROUP::END)
        return;

    CGameInstance::GetInstance()->Add_Collider(m_eGroup, this);
}

_bool CCollider::Intersect(CCollider* pOther)
{
    if (nullptr == pOther || nullptr == m_pBounding || nullptr == pOther->m_pBounding)
        return false;

    return m_pBounding->Intersect(pOther->m_eBoundingType, pOther->m_pBounding);
}

void CCollider::Set_Radius(_float fRadius)
{
    if (COLLIDER::SPHERE != m_eBoundingType || nullptr == m_pBounding)
        return;

    static_cast<CBounding_Sphere*>(m_pBounding)->Set_Radius(fRadius);
}

#ifdef _DEBUG
HRESULT CCollider::Render(PrimitiveBatch<VertexPositionColor>* pBatch)
{
    if (m_pBounding)
        m_pBounding->Render(pBatch);
    return S_OK;
}
#endif

CCollider* CCollider::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CCollider* pInstance = new CCollider(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CCollider");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CComponent* CCollider::Clone(void* pArg)
{
    CCollider* pInstance = new CCollider(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CCollider");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CCollider::Free()
{
    __super::Free();

    Clear_Callbacks();
    Safe_Release(m_pBounding);
}