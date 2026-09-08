#include "NavMesh.h"
#include "Cell.h"

namespace
{
	static constexpr char NAVDATA_MAGIC[4] = { 'S', 'L', 'N', 'M' };
	static constexpr _uint NAVDATA_VERSION_MIN = { 1 };
	static constexpr _uint NAVDATA_VERSION_LATEST = { 1 };

	static _bool Read_Block(FILE* fp, void* pData, size_t iElementSize, size_t iElementCount)
	{
		return nullptr != fp &&
			nullptr != pData &&
			iElementCount == fread(pData, iElementSize, iElementCount, fp);
	}

	static _bool Write_Block(FILE* fp, const void* pData, size_t iElementSize, size_t iElementCount)
	{
		return nullptr != fp &&
			nullptr != pData &&
			iElementCount == fwrite(pData, iElementSize, iElementCount, fp);
	}
}


CNavMesh::CNavMesh(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CComponent{ pDevice, pContext }
{
}

CNavMesh::CNavMesh(const CNavMesh& Prototype)
	: CComponent{ Prototype }
	, m_Vertices{ Prototype.m_Vertices }
	, m_CellDescs{ Prototype.m_CellDescs }
{
}

HRESULT CNavMesh::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CNavMesh::Initialize(void* pArg)
{
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (nullptr != pArg)
	{
		NAVMESH_DESC* pDesc = static_cast<NAVMESH_DESC*>(pArg);

		if (nullptr != pDesc->pInitialSnapshot)
			return Restore_Snapshot(*pDesc->pInitialSnapshot);
	}

	if (FAILED(Rebuild_CellObjects()))
		return E_FAIL;

	return S_OK;
}

const CCell* CNavMesh::Get_Cell(_int iCellIndex) const
{
	if (iCellIndex < 0)
		return nullptr;

	if (static_cast<size_t>(iCellIndex) >= m_Cells.size())
		return nullptr;

	return m_Cells[iCellIndex];
}

_int CNavMesh::Find_Vertex(const _float3& vPosition, _float fSnapRadius) const
{
	const _float fSnapRadiusSq = fSnapRadius * fSnapRadius;

	for (_uint i = 0; i < static_cast<_uint>(m_Vertices.size()); ++i)
	{
		const _float3& vVertex = m_Vertices[i];

		const _float fDistanceSq =
			(vVertex.x - vPosition.x) * (vVertex.x - vPosition.x) +
			(vVertex.y - vPosition.y) * (vVertex.y - vPosition.y) +
			(vVertex.z - vPosition.z) * (vVertex.z - vPosition.z);

		if (fDistanceSq <= fSnapRadiusSq)
			return static_cast<_int>(i);
	}

	return INVALID_INDEX;
}

_int CNavMesh::Add_Vertex(const _float3& vPosition)
{
	m_Vertices.push_back(vPosition);

	return static_cast<_int>(m_Vertices.size() - 1);
}

_int CNavMesh::Find_OrAddVertex(const _float3& vPosition, _float fSnapRadius)
{
	const _int iVertexIndex = Find_Vertex(vPosition, fSnapRadius);

	if (INVALID_INDEX != iVertexIndex)
		return iVertexIndex;

	return Add_Vertex(vPosition);
}

HRESULT CNavMesh::Move_Vertex(_int iVertexIndex, const _float3& vPosition, _float fMergeRejectRadius)
{
	if (false == Is_ValidVertexIndex(iVertexIndex))
		return E_FAIL;

	if (fMergeRejectRadius > 0.f)
	{
		const _int iFoundVertexIndex = Find_Vertex(vPosition, fMergeRejectRadius);

		if (INVALID_INDEX != iFoundVertexIndex &&
			iFoundVertexIndex != iVertexIndex)
			return E_FAIL;
	}

	vector<_float3> BackupVertices = m_Vertices;

	m_Vertices[iVertexIndex] = vPosition;

	if (FAILED(Rebuild_Neighbors()))
	{
		m_Vertices = BackupVertices;
		Rebuild_Neighbors();
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CNavMesh::Add_Cell(const NAVMESH_CELL& Cell, _int* pOutCellIndex)
{
	if (nullptr != pOutCellIndex)
		*pOutCellIndex = INVALID_INDEX;

	const _int iCellIndex = static_cast<_int>(m_CellDescs.size());

	CCell* pValidationCell = CCell::Create(iCellIndex, &m_Vertices, Cell);
	if (nullptr == pValidationCell)
		return E_FAIL;

	Safe_Release(pValidationCell);

	m_CellDescs.push_back(Cell);

	if (FAILED(Rebuild_Neighbors()))
	{
		m_CellDescs.pop_back();
		Rebuild_Neighbors();
		return E_FAIL;
	}

	if (nullptr != pOutCellIndex)
		*pOutCellIndex = iCellIndex;

	return S_OK;
}

HRESULT CNavMesh::Try_AddCell(_int iVertex0, _int iVertex1, _int iVertex2, _int* pOutCellIndex)
{
	if (nullptr != pOutCellIndex)
		*pOutCellIndex = INVALID_INDEX;

	if (false == Is_ValidVertexIndex(iVertex0) ||
		false == Is_ValidVertexIndex(iVertex1) ||
		false == Is_ValidVertexIndex(iVertex2))
		return E_FAIL;

	if (iVertex0 == iVertex1 ||
		iVertex1 == iVertex2 ||
		iVertex2 == iVertex0)
		return E_FAIL;

	_float fSignedArea = Compute_CellSignedArea_XZ(iVertex0, iVertex1, iVertex2);
	if (fabsf(fSignedArea) <= NAVMESH_MIN_CELL_AREA)
		return E_FAIL;

	NAVMESH_CELL Cell{};
	Cell.iVertexIndices[0] = iVertex0;
	Cell.iVertexIndices[1] = iVertex1;
	Cell.iVertexIndices[2] = iVertex2;

	// 위에서 봤을 때 CCW 기준으로 통일한다.
	if (fSignedArea < 0.f)
	{
		std::swap(Cell.iVertexIndices[1], Cell.iVertexIndices[2]);
		fSignedArea = -fSignedArea;
	}

	return Add_Cell(Cell, pOutCellIndex);
}

HRESULT CNavMesh::Remove_Cell(_int iCellIndex)
{
	if (false == Is_ValidCellIndex(iCellIndex))
		return E_FAIL;

	vector<NAVMESH_CELL> BackupCells = m_CellDescs;

	m_CellDescs.erase(m_CellDescs.begin() + iCellIndex);

	if (FAILED(Rebuild_Neighbors()))
	{
		m_CellDescs = BackupCells;
		Rebuild_Neighbors();
		return E_FAIL;
	}

	return S_OK;
}

void CNavMesh::Clear()
{
	Release_CellObjects();

	m_CellDescs.clear();
	m_Vertices.clear();
}

HRESULT CNavMesh::Save_NavData(const _tchar* pNavDataPath) const
{
	NAVMESH_SNAPSHOT Snapshot = Capture_Snapshot();

	return Save_NavDataSnapshot(pNavDataPath, Snapshot);
}

HRESULT CNavMesh::Load_NavData(const _tchar* pNavDataPath)
{
	NAVMESH_SNAPSHOT Snapshot{};

	if (FAILED(Load_NavDataSnapshot(pNavDataPath, &Snapshot)))
		return E_FAIL;

	NAVMESH_SNAPSHOT Backup = Capture_Snapshot();

	if (FAILED(Restore_Snapshot(Snapshot)))
	{
		Restore_Snapshot(Backup);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CNavMesh::Save_NavDataSnapshot(const _tchar* pNavDataPath, const NAVMESH_SNAPSHOT& Snapshot)
{
	if (nullptr == pNavDataPath || '\0' == pNavDataPath[0])
		return E_FAIL;

	FILE* fp = nullptr;
	if (0 != _wfopen_s(&fp, pNavDataPath, L"wb") || nullptr == fp)
		return E_FAIL;

	const _uint iVersion = NAVDATA_VERSION_LATEST;
	const _uint iNumVertices = static_cast<_uint>(Snapshot.Vertices.size());
	const _uint iNumCells = static_cast<_uint>(Snapshot.Cells.size());

	if (false == Write_Block(fp, NAVDATA_MAGIC, sizeof(char), 4) ||
		false == Write_Block(fp, &iVersion, sizeof(_uint), 1) ||
		false == Write_Block(fp, &iNumVertices, sizeof(_uint), 1) ||
		false == Write_Block(fp, &iNumCells, sizeof(_uint), 1))
	{
		fclose(fp);
		return E_FAIL;
	}

	if (iNumVertices > 0 &&
		false == Write_Block(fp, Snapshot.Vertices.data(), sizeof(_float3), iNumVertices))
	{
		fclose(fp);
		return E_FAIL;
	}

	for (_uint i = 0; i < iNumCells; ++i)
	{
		const NAVMESH_CELL& Cell = Snapshot.Cells[i];

		if (false == Write_Block(fp, Cell.iVertexIndices, sizeof(_int), ETOUI(NAVMESH_POINT::END)))
		{
			fclose(fp);
			return E_FAIL;
		}
	}

	fclose(fp);

	return S_OK;
}

HRESULT CNavMesh::Load_NavDataSnapshot(const _tchar* pNavDataPath, NAVMESH_SNAPSHOT* pOutSnapshot)
{
	if (nullptr == pNavDataPath || '\0' == pNavDataPath[0] || nullptr == pOutSnapshot)
		return E_FAIL;

	FILE* fp = nullptr;
	if (0 != _wfopen_s(&fp, pNavDataPath, L"rb") || nullptr == fp)
		return E_FAIL;

	char szMagic[4] = {};
	_uint iVersion = {};
	_uint iNumVertices = {};
	_uint iNumCells = {};

	if (false == Read_Block(fp, szMagic, sizeof(char), 4) ||
		0 != memcmp(szMagic, NAVDATA_MAGIC, 4) ||
		false == Read_Block(fp, &iVersion, sizeof(_uint), 1) ||
		iVersion < NAVDATA_VERSION_MIN ||
		iVersion > NAVDATA_VERSION_LATEST ||
		false == Read_Block(fp, &iNumVertices, sizeof(_uint), 1) ||
		false == Read_Block(fp, &iNumCells, sizeof(_uint), 1))
	{
		fclose(fp);
		return E_FAIL;
	}

	NAVMESH_SNAPSHOT Snapshot{};
	Snapshot.Vertices.resize(iNumVertices);
	Snapshot.Cells.resize(iNumCells);

	if (iNumVertices > 0 &&
		false == Read_Block(fp, Snapshot.Vertices.data(), sizeof(_float3), iNumVertices))
	{
		fclose(fp);
		return E_FAIL;
	}

	for (_uint i = 0; i < iNumCells; ++i)
	{
		NAVMESH_CELL& Cell = Snapshot.Cells[i];

		if (false == Read_Block(fp, Cell.iVertexIndices, sizeof(_int), ETOUI(NAVMESH_POINT::END)))
		{
			fclose(fp);
			return E_FAIL;
		}
	}

	fclose(fp);

	*pOutSnapshot = Snapshot;

	return S_OK;
}

NAVMESH_SNAPSHOT CNavMesh::Capture_Snapshot() const
{
	NAVMESH_SNAPSHOT Snapshot{};
	Snapshot.Vertices = m_Vertices;
	Snapshot.Cells = m_CellDescs;

	return Snapshot;
}

HRESULT CNavMesh::Restore_Snapshot(const NAVMESH_SNAPSHOT& Snapshot)
{
	Release_CellObjects();

	m_Vertices = Snapshot.Vertices;
	m_CellDescs = Snapshot.Cells;

	if (FAILED(Rebuild_Neighbors()))
		return E_FAIL;

	return S_OK;
}

_int CNavMesh::Find_Cell(const _float3& vPosition) const
{
	for (_uint i = 0; i < static_cast<_uint>(m_Cells.size()); ++i)
	{
		if (nullptr == m_Cells[i])
			continue;

		if (true == m_Cells[i]->Is_In(vPosition))
			return static_cast<_int>(i);
	}

	return INVALID_INDEX;
}

_bool CNavMesh::Try_Move(_int* pCurrentCellIndex, const _float3& vCandidatePosition, _float3* pOutAdjustedPosition) const
{
	if (nullptr == pCurrentCellIndex)
		return false;

	if (nullptr != pOutAdjustedPosition)
		*pOutAdjustedPosition = vCandidatePosition;

	_int iCurrentCellIndex = *pCurrentCellIndex;

	if (false == Is_ValidCellIndex(iCurrentCellIndex))
	{
		iCurrentCellIndex = Find_Cell(vCandidatePosition);

		if (false == Is_ValidCellIndex(iCurrentCellIndex))
			return false;

		*pCurrentCellIndex = iCurrentCellIndex;
	}

	_int iNeighborIndex = INVALID_INDEX;

	if (true == m_Cells[iCurrentCellIndex]->Is_In(vCandidatePosition, &iNeighborIndex))
	{
		if (nullptr != pOutAdjustedPosition)
		{
			*pOutAdjustedPosition = vCandidatePosition;
			pOutAdjustedPosition->y = m_Cells[iCurrentCellIndex]->Compute_Height(vCandidatePosition);
		}

		return true;
	}

	const _uint iMaxHopCount = static_cast<_uint>(m_Cells.size());
	_uint iHopCount = 0;

	while (true == Is_ValidCellIndex(iNeighborIndex) && iHopCount < iMaxHopCount)
	{
		++iHopCount;

		const _int iNextCellIndex = iNeighborIndex;
		iNeighborIndex = INVALID_INDEX;

		if (true == m_Cells[iNextCellIndex]->Is_In(vCandidatePosition, &iNeighborIndex))
		{
			*pCurrentCellIndex = iNextCellIndex;

			if (nullptr != pOutAdjustedPosition)
			{
				*pOutAdjustedPosition = vCandidatePosition;
				pOutAdjustedPosition->y = m_Cells[iNextCellIndex]->Compute_Height(vCandidatePosition);
			}

			return true;
		}
	}

	return false;
}

_float CNavMesh::Compute_Height(_int iCellIndex, const _float3& vPosition) const
{
	if (false == Is_ValidCellIndex(iCellIndex))
		return vPosition.y;

	return m_Cells[iCellIndex]->Compute_Height(vPosition);
}

HRESULT CNavMesh::Rebuild_Neighbors()
{
	for (auto& CellDesc : m_CellDescs)
	{
		for (_uint iLine = 0; iLine < ETOUI(NAVMESH_LINE::END); ++iLine)
			CellDesc.iNeighborIndices[iLine] = INVALID_INDEX;
	}

	if (FAILED(Rebuild_CellObjects()))
		return E_FAIL;

	for (_uint i = 0; i < static_cast<_uint>(m_Cells.size()); ++i)
	{
		for (_uint j = i + 1; j < static_cast<_uint>(m_Cells.size()); ++j)
		{
			_uint iLine = ETOUI(NAVMESH_LINE::END);
			_uint iOtherLine = ETOUI(NAVMESH_LINE::END);

			if (false == m_Cells[i]->Shares_Edge(m_Cells[j], &iLine, &iOtherLine))
				continue;

			if (iLine >= ETOUI(NAVMESH_LINE::END) ||
				iOtherLine >= ETOUI(NAVMESH_LINE::END))
				continue;

			m_CellDescs[i].iNeighborIndices[iLine] = static_cast<_int>(j);
			m_CellDescs[j].iNeighborIndices[iOtherLine] = static_cast<_int>(i);
		}
	}

	if (FAILED(Rebuild_CellObjects()))
		return E_FAIL;

	return S_OK;
}

HRESULT CNavMesh::Rebuild_CellObjects()
{
	Release_CellObjects();

	m_Cells.reserve(m_CellDescs.size());

	for (_uint i = 0; i < static_cast<_uint>(m_CellDescs.size()); ++i)
	{
		CCell* pCell = CCell::Create(static_cast<_int>(i), &m_Vertices, m_CellDescs[i]);
		if (nullptr == pCell)
		{
			Release_CellObjects();
			return E_FAIL;
		}

		m_Cells.push_back(pCell);
	}

	return S_OK;
}

_bool CNavMesh::Is_ValidVertexIndex(_int iVertexIndex) const
{
	if (iVertexIndex < 0)
		return false;

	return static_cast<size_t>(iVertexIndex) < m_Vertices.size();
}

_bool CNavMesh::Is_ValidCellIndex(_int iCellIndex) const
{
	if (iCellIndex < 0)
		return false;

	return static_cast<size_t>(iCellIndex) < m_Cells.size() &&
		nullptr != m_Cells[iCellIndex];
}

_float CNavMesh::Compute_CellSignedArea_XZ(_int iVertex0, _int iVertex1, _int iVertex2) const
{
	if (false == Is_ValidVertexIndex(iVertex0) ||
		false == Is_ValidVertexIndex(iVertex1) ||
		false == Is_ValidVertexIndex(iVertex2))
		return 0.f;

	const _float3& vA = m_Vertices[iVertex0];
	const _float3& vB = m_Vertices[iVertex1];
	const _float3& vC = m_Vertices[iVertex2];

	_vector vAB = XMVectorSubtract(XMLoadFloat3(&vB), XMLoadFloat3(&vA));
	_vector vAC = XMVectorSubtract(XMLoadFloat3(&vC), XMLoadFloat3(&vA));
	_vector vCross = XMVector3Cross(vAB, vAC);

	return XMVectorGetY(vCross) * 0.5f;
}

void CNavMesh::Release_CellObjects()
{
	for (auto& pCell : m_Cells)
		Safe_Release(pCell);

	m_Cells.clear();
}

CNavMesh* CNavMesh::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CNavMesh* pInstance = new CNavMesh(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CNavMesh");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CNavMesh::Clone(void* pArg)
{
	CNavMesh* pInstance = new CNavMesh(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CNavMesh");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CNavMesh::Free()
{
	__super::Free();

	Release_CellObjects();

	m_CellDescs.clear();
	m_Vertices.clear();
}
