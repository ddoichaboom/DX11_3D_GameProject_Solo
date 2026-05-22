#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class CGameObject;
class CBounding;

class ENGINE_DLL CCollider final : public CComponent
{
public:
    using HitCallback = function<void(CCollider* /*pOther*/)>;

    typedef struct tagColliderDesc
    {
        COLLIDER            eBoundingType = { COLLIDER::END };
        COLLISION_GROUP     eGroup = { COLLISION_GROUP::END };
        _float3             vCenter = { 0.f, 0.f, 0.f };
        _float3             vSize = { 1.f, 1.f, 1.f };      // AABB / OBB
        _float3             vRadians = { 0.f, 0.f, 0.f };   // OBB
        _float              fRadius = { 0.5f };             // Sphere
        CGameObject* pOwner = { nullptr };
    } COLLIDER_DESC;

private:
    CCollider(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CCollider(const CCollider& Prototype);
    virtual ~CCollider() = default;

public:
    virtual HRESULT     Initialize_Prototype() override;
    virtual HRESULT     Initialize(void* pArg) override;

    // 매 프레임 Owner 의 WorldMatrix 로 호출 → Bounding 갱신
    void                Update(_fmatrix WorldMatrix);

    // GameInstance 경유 Collision_Manager 에 등록
    void                Register();

    _bool               Intersect(CCollider* pOther);

    void                Set_Radius(_float fRadius);

#ifdef _DEBUG
    HRESULT             Render(PrimitiveBatch<VertexPositionColor>* pBatch);
#endif

    void                Set_OnHitEnter(HitCallback fn) { m_OnHitEnter = std::move(fn); }
    void                Set_OnHitStay(HitCallback fn) { m_OnHitStay = std::move(fn); }
    void                Set_OnHitExit(HitCallback fn) { m_OnHitExit = std::move(fn); }
    void                Clear_Callbacks()
    {
        m_OnHitEnter = nullptr;
        m_OnHitStay = nullptr;
        m_OnHitExit = nullptr;
    }

    void                Notify_HitEnter(CCollider* pOther) { if (m_OnHitEnter) m_OnHitEnter(pOther); }
    void                Notify_HitStay(CCollider* pOther) { if (m_OnHitStay)  m_OnHitStay(pOther); }
    void                Notify_HitExit(CCollider* pOther) { if (m_OnHitExit)  m_OnHitExit(pOther); }

    COLLISION_GROUP     Get_Group()         const { return m_eGroup; }
    COLLIDER            Get_BoundingType()  const { return m_eBoundingType; }
    CGameObject*        Get_Owner()         const { return m_pOwner; }
    CBounding*          Get_Bounding()      const { return m_pBounding; }

private:
    CBounding*          m_pBounding = { nullptr };
    COLLIDER            m_eBoundingType = { COLLIDER::END };
    COLLISION_GROUP     m_eGroup = { COLLISION_GROUP::END };
    CGameObject*        m_pOwner = { nullptr };

    HitCallback         m_OnHitEnter;
    HitCallback         m_OnHitStay;
    HitCallback         m_OnHitExit;

public:
    static CCollider*   Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CComponent* Clone(void* pArg) override;
    virtual void        Free() override;
};

NS_END