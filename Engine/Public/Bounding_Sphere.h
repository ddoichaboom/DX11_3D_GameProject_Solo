#pragma once

#include "Bounding.h"

NS_BEGIN(Engine)

class CBounding_Sphere final : public CBounding
{
public:
    typedef struct tagBoundingSphereDesc : public CBounding::BOUNDING_DESC
    {
        _float      fRadius;
    } BOUNDING_SPHERE_DESC;

private:
    CBounding_Sphere(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual ~CBounding_Sphere() = default;

public:
    const BoundingSphere*       Get_Desc() const { return m_pDesc; }
    void                        Set_Radius(_float fRadius);

public:
    virtual HRESULT             Initialize(const CBounding::BOUNDING_DESC* pBoundingDesc) override;
    virtual void                Update(_fmatrix WorldMatrix) override;

public:
    virtual _bool               Intersect(COLLIDER eTargetType, CBounding* pTarget) override;

#ifdef _DEBUG
public:
    virtual HRESULT             Render(PrimitiveBatch<VertexPositionColor>* pBatch) override;
#endif

private:
    BoundingSphere*             m_pOriginalDesc = { nullptr };
    BoundingSphere*             m_pDesc = { nullptr };

public:
    static CBounding_Sphere*    Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const CBounding::BOUNDING_DESC* pBoundingDesc);
    virtual void                Free() override;
};

NS_END