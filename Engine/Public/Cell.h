#pragma once

#include "Base.h"
#include "NavMesh_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL CCell final : public CBase
{
private:
	CCell();
	virtual ~CCell() = default;

public:
	HRESULT						Initialize(_int iCellIndex, const vector<_float3>* pVertices, const NAVMESH_CELL& Cell);

public:
	_int						Get_Index() const { return m_iCellIndex; }
	const NAVMESH_CELL&			Get_Cell() const { return m_Cell; }

	_int						Get_VertexIndex(_uint iPointIndex) const;
	_int						Get_NeighborIndex(_uint iLineIndex) const;
	void						Set_NeighborIndex(_uint iLineIndex, _int iNeighborIndex);

	void						Get_EdgeVertexIndices(_uint iLineIndex, _int* pOutStart, _int* pOutEnd) const;

	_bool						Is_In(const _float3& vPosition, _int* pOutNeighborIndex = nullptr) const;
	_bool						Shares_Edge(const CCell* pOther, _uint* pOutLineIndex = nullptr, _uint* pOutOtherLineIndex = nullptr) const;

	_float						Compute_Height(const _float3& vPosition) const;
	_float						Compute_SignedArea_XZ() const;
	_float						Compute_Area_XZ() const;
	_float3						Get_Center() const;

private:
	HRESULT						Validate() const;
	void						Rebuild_Plane();
	_float3						Get_Vertex(_uint iPointIndex) const;

private:
	const vector<_float3>*		m_pVertices = { nullptr };
	NAVMESH_CELL				m_Cell = {};
	_int						m_iCellIndex = { INVALID_INDEX };
	_float4						m_vPlane = { 0.f, 1.f, 0.f, 0.f };

public:
	static CCell*				Create(_int iCellIndex, const vector<_float3>* pVertices, const NAVMESH_CELL& Cell);
	virtual void				Free() override;
};

NS_END