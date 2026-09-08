#include "Cell.h"

namespace
{
	static constexpr _float NAVMESH_SIDE_EPSILON = { 0.001f };
	static constexpr _float NAVMESH_PLANE_EPSILON = { 0.00001f };

	static _uint NextPoint(_uint iPointIndex)
	{
		return (iPointIndex + 1) % ETOUI(NAVMESH_POINT::END);
	}
}

CCell::CCell()
{
}

HRESULT CCell::Initialize(_int iCellIndex, const vector<_float3>* pVertices, const NAVMESH_CELL& Cell)
{
	m_iCellIndex	= iCellIndex;
	m_pVertices		= pVertices;
	m_Cell			= Cell;

	if (FAILED(Validate()))
		return E_FAIL;

	Rebuild_Plane();

	return S_OK;
}

_int CCell::Get_VertexIndex(_uint iPointIndex) const
{
	if (iPointIndex >= ETOUI(NAVMESH_POINT::END))
		return INVALID_INDEX;

	return m_Cell.iVertexIndices[iPointIndex];
}

_int CCell::Get_NeighborIndex(_uint iLineIndex) const
{
	if (iLineIndex >= ETOUI(NAVMESH_LINE::END))
		return INVALID_INDEX;

	return m_Cell.iNeighborIndices[iLineIndex];
}

void CCell::Set_NeighborIndex(_uint iLineIndex, _int iNeighborIndex)
{
	if (iLineIndex >= ETOUI(NAVMESH_LINE::END))
		return;

	m_Cell.iNeighborIndices[iLineIndex] = iNeighborIndex;
}

void CCell::Get_EdgeVertexIndices(_uint iLineIndex, _int* pOutStart, _int* pOutEnd) const
{
	if (nullptr != pOutStart)
		*pOutStart = INVALID_INDEX;

	if (nullptr != pOutEnd)
		*pOutEnd = INVALID_INDEX;

	if (iLineIndex >= ETOUI(NAVMESH_LINE::END))
		return;

	const _uint iStart = iLineIndex;
	const _uint iEnd = NextPoint(iLineIndex);

	if (nullptr != pOutStart)
		*pOutStart = m_Cell.iVertexIndices[iStart];

	if (nullptr != pOutEnd)
		*pOutEnd = m_Cell.iVertexIndices[iEnd];
}

_bool CCell::Is_In(const _float3& vPosition, _int* pOutNeighborIndex) const
{
	if (nullptr != pOutNeighborIndex)
		*pOutNeighborIndex = INVALID_INDEX;

	const _float fSignedArea = Compute_SignedArea_XZ();
	const _float fWindingSign = (fSignedArea >= 0.f) ? 1.f : -1.f;

	for (_uint iLine = 0; iLine < ETOUI(NAVMESH_LINE::END); ++iLine)
	{
		const _float3 vStart = Get_Vertex(iLine);
		const _float3 vEnd = Get_Vertex(NextPoint(iLine));

		_vector vEdge = XMVectorSubtract(XMLoadFloat3(&vEnd), XMLoadFloat3(&vStart));
		_vector vToPosition = XMVectorSubtract(XMLoadFloat3(&vPosition), XMLoadFloat3(&vStart));
		_vector vCross = XMVector3Cross(vEdge, vToPosition);

		const _float fSide = XMVectorGetY(vCross) * fWindingSign;

		if (fSide < -NAVMESH_SIDE_EPSILON)
		{
			if (nullptr != pOutNeighborIndex)
				*pOutNeighborIndex = m_Cell.iNeighborIndices[iLine];

			return false;
		}
	}

	return true;
}

_bool CCell::Shares_Edge(const CCell* pOther, _uint* pOutLineIndex, _uint* pOutOtherLineIndex) const
{
	if (nullptr != pOutLineIndex)
		*pOutLineIndex = ETOUI(NAVMESH_LINE::END);

	if (nullptr != pOutOtherLineIndex)
		*pOutOtherLineIndex = ETOUI(NAVMESH_LINE::END);

	if (nullptr == pOther)
		return false;

	for (_uint iLine = 0; iLine < ETOUI(NAVMESH_LINE::END); ++iLine)
	{
		_int iStart = INVALID_INDEX;
		_int iEnd = INVALID_INDEX;
		Get_EdgeVertexIndices(iLine, &iStart, &iEnd);

		for (_uint iOtherLine = 0; iOtherLine < ETOUI(NAVMESH_LINE::END); ++iOtherLine)
		{
			_int iOtherStart = INVALID_INDEX;
			_int iOtherEnd = INVALID_INDEX;
			pOther->Get_EdgeVertexIndices(iOtherLine, &iOtherStart, &iOtherEnd);

			const _bool bSameDirection =
				(iStart == iOtherStart && iEnd == iOtherEnd);

			const _bool bOppositeDirection =
				(iStart == iOtherEnd && iEnd == iOtherStart);

			if (true == bSameDirection || true == bOppositeDirection)
			{
				if (nullptr != pOutLineIndex)
					*pOutLineIndex = iLine;

				if (nullptr != pOutOtherLineIndex)
					*pOutOtherLineIndex = iOtherLine;

				return true;
			}
		}
	}

	return false;
}

_float CCell::Compute_Height(const _float3& vPosition) const
{
	if (fabsf(m_vPlane.y) <= NAVMESH_PLANE_EPSILON)
		return vPosition.y;

	return -(m_vPlane.x * vPosition.x + m_vPlane.z * vPosition.z + m_vPlane.w) / m_vPlane.y;
}

_float CCell::Compute_SignedArea_XZ() const
{
	const _float3 vA = Get_Vertex(ETOUI(NAVMESH_POINT::A));
	const _float3 vB = Get_Vertex(ETOUI(NAVMESH_POINT::B));
	const _float3 vC = Get_Vertex(ETOUI(NAVMESH_POINT::C));

	_vector vAB = XMVectorSubtract(XMLoadFloat3(&vB), XMLoadFloat3(&vA));
	_vector vAC = XMVectorSubtract(XMLoadFloat3(&vC), XMLoadFloat3(&vA));
	_vector vCross = XMVector3Cross(vAB, vAC);

	return XMVectorGetY(vCross) * 0.5f;
}

_float CCell::Compute_Area_XZ() const
{
	return fabsf(Compute_SignedArea_XZ());
}

_float3 CCell::Get_Center() const
{
	const _float3 vA = Get_Vertex(ETOUI(NAVMESH_POINT::A));
	const _float3 vB = Get_Vertex(ETOUI(NAVMESH_POINT::B));
	const _float3 vC = Get_Vertex(ETOUI(NAVMESH_POINT::C));

	return _float3{
			(vA.x + vB.x + vC.x) / 3.f,
			(vA.y + vB.y + vC.y) / 3.f,
			(vA.z + vB.z + vC.z) / 3.f
	};
}

HRESULT CCell::Validate() const
{
	if (nullptr == m_pVertices)
		return E_FAIL;

	for (_uint i = 0; i < ETOUI(NAVMESH_POINT::END); ++i)
	{
		const _int iVertexIndex = m_Cell.iVertexIndices[i];

		if (iVertexIndex < 0)
			return E_FAIL;

		if (static_cast<size_t>(iVertexIndex) >= m_pVertices->size())
			return E_FAIL;
	}

	if (m_Cell.iVertexIndices[0] == m_Cell.iVertexIndices[1] ||
		m_Cell.iVertexIndices[1] == m_Cell.iVertexIndices[2] ||
		m_Cell.iVertexIndices[2] == m_Cell.iVertexIndices[0])
		return E_FAIL;

	if (Compute_Area_XZ() <= NAVMESH_MIN_CELL_AREA)
		return E_FAIL;

	return S_OK;
}

void CCell::Rebuild_Plane()
{
	const _float3 vA = Get_Vertex(ETOUI(NAVMESH_POINT::A));
	const _float3 vB = Get_Vertex(ETOUI(NAVMESH_POINT::B));
	const _float3 vC = Get_Vertex(ETOUI(NAVMESH_POINT::C));

	_vector vPlane = XMPlaneFromPoints(
		XMLoadFloat3(&vA),
		XMLoadFloat3(&vB),
		XMLoadFloat3(&vC));

	XMStoreFloat4(&m_vPlane, vPlane);
}

_float3 CCell::Get_Vertex(_uint iPointIndex) const
{
	if (nullptr == m_pVertices ||
		iPointIndex >= ETOUI(NAVMESH_POINT::END))
		return _float3{};

	const _int iVertexIndex = m_Cell.iVertexIndices[iPointIndex];

	if (iVertexIndex < 0 ||
		static_cast<size_t>(iVertexIndex) >= m_pVertices->size())
		return _float3{};

	return (*m_pVertices)[iVertexIndex];
}

CCell* CCell::Create(_int iCellIndex, const vector<_float3>* pVertices, const NAVMESH_CELL& Cell)
{
	CCell* pInstance = new CCell();

	if (FAILED(pInstance->Initialize(iCellIndex, pVertices, Cell)))
	{
		MSG_BOX("Failed to Created : CCell");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCell::Free()
{
	__super::Free();
}
