#include "VIBuffer_Rect_Instance.h"

CVIBuffer_Rect_Instance::CVIBuffer_Rect_Instance(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CVIBuffer_Instance{ pDevice, pContext }
{
}

CVIBuffer_Rect_Instance::CVIBuffer_Rect_Instance(const CVIBuffer_Rect_Instance& Prototype)
	: CVIBuffer_Instance{ Prototype }
	, m_iMaxInstanceCount{ Prototype.m_iMaxInstanceCount }
{
}

HRESULT CVIBuffer_Rect_Instance::Initialize_Prototype(_uint iMaxInstanceCount)
{
	if (0 == iMaxInstanceCount)
		return E_FAIL;

	m_iNumVertexBuffers = 2;
	m_iNumVertices = 4;
	m_iVertexStride = sizeof(VTXTEX);

	m_iNumIndices = 6;
	m_iIndexStride = 2;
	m_eIndexFormat = DXGI_FORMAT_R16_UINT;
	m_ePrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_iIndexCountPerInstance = m_iNumIndices;
	m_iMaxInstanceCount = iMaxInstanceCount;

	D3D11_BUFFER_DESC VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = m_iVertexStride * m_iNumVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;
	VertexBufferDesc.StructureByteStride = m_iVertexStride;

	VTXTEX* pVertices = new VTXTEX[m_iNumVertices];
	ZeroMemory(pVertices, sizeof(VTXTEX) * m_iNumVertices);

	pVertices[0].vPosition = _float3(-0.5f, 0.5f, 0.f);
	pVertices[0].vTexcoord = _float2(0.f, 0.f);

	pVertices[1].vPosition = _float3(0.5f, 0.5f, 0.f);
	pVertices[1].vTexcoord = _float2(1.f, 0.f);

	pVertices[2].vPosition = _float3(0.5f, -0.5f, 0.f);
	pVertices[2].vTexcoord = _float2(1.f, 1.f);

	pVertices[3].vPosition = _float3(-0.5f, -0.5f, 0.f);
	pVertices[3].vTexcoord = _float2(0.f, 1.f);

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = pVertices;

	if (FAILED(m_pDevice->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &m_pVB)))
	{
		Safe_Delete_Array(pVertices);
		return E_FAIL;
	}

	Safe_Delete_Array(pVertices);

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = m_iIndexStride * m_iNumIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.CPUAccessFlags = 0;
	IndexBufferDesc.MiscFlags = 0;
	IndexBufferDesc.StructureByteStride = m_iIndexStride;

	_ushort* pIndices = new _ushort[m_iNumIndices];
	ZeroMemory(pIndices, sizeof(_ushort) * m_iNumIndices);

	pIndices[0] = 0;
	pIndices[1] = 1;
	pIndices[2] = 2;
	pIndices[3] = 0;
	pIndices[4] = 2;
	pIndices[5] = 3;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = pIndices;

	if (FAILED(m_pDevice->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &m_pIB)))
	{
		Safe_Delete_Array(pIndices);
		return E_FAIL;
	}

	Safe_Delete_Array(pIndices);

	return S_OK;
}

HRESULT CVIBuffer_Rect_Instance::Initialize(void* pArg)
{
	RECT_INSTANCE_DESC* pDesc = static_cast<RECT_INSTANCE_DESC*>(pArg);

	_uint iMaxInstanceCount = m_iMaxInstanceCount;
	const VTXRECT_INSTANCE* pInitialInstances = nullptr;
	_uint iInitialInstanceCount = 0;

	if (nullptr != pDesc)
	{
		if (0 != pDesc->iMaxInstanceCount)
			iMaxInstanceCount = pDesc->iMaxInstanceCount;

		pInitialInstances = pDesc->pInitialInstances;
		iInitialInstanceCount = pDesc->iInitialInstanceCount;
	}

	if (0 == iMaxInstanceCount)
		return E_FAIL;

	m_iMaxInstanceCount = iMaxInstanceCount;
	m_iInstanceStride = sizeof(VTXRECT_INSTANCE);
	m_iIndexCountPerInstance = m_iNumIndices;

	m_Instances.resize(m_iMaxInstanceCount);
	ZeroMemory(m_Instances.data(), sizeof(VTXRECT_INSTANCE) * m_Instances.size());

	_uint iNumInstances = min(iInitialInstanceCount, m_iMaxInstanceCount);

	if (nullptr != pInitialInstances && 0 != iNumInstances)
		memcpy(m_Instances.data(), pInitialInstances, sizeof(VTXRECT_INSTANCE) * iNumInstances);

	if (FAILED(Create_InstanceBuffer(m_Instances.data(), m_iMaxInstanceCount, sizeof(VTXRECT_INSTANCE))))
		return E_FAIL;

	m_iNumInstances = iNumInstances;

	return S_OK;
}

HRESULT CVIBuffer_Rect_Instance::Set_Instances(const vector<VTXRECT_INSTANCE>& Instances)
{
	if (Instances.size() > m_iMaxInstanceCount)
		return E_FAIL;

	return Set_Instances(Instances.data(), static_cast<_uint>(Instances.size()));
}

HRESULT CVIBuffer_Rect_Instance::Set_Instances(const VTXRECT_INSTANCE* pInstances, _uint iNumInstances)
{
	if (iNumInstances > m_iMaxInstanceCount)
		return E_FAIL;

	if (0 != iNumInstances && nullptr == pInstances)
		return E_FAIL;

	if (0 != iNumInstances)
		memcpy(m_Instances.data(), pInstances, sizeof(VTXRECT_INSTANCE) * iNumInstances);

	return Update_InstanceBuffer(m_Instances.data(), iNumInstances, sizeof(VTXRECT_INSTANCE));
}

HRESULT CVIBuffer_Rect_Instance::Update_Billboard(const vector<_float4>& Positions, const _float2& vSize, const _float4& vTexInfo, const _float4& vColor, _fmatrix ViewMatrix)
{
	if (Positions.size() > m_iMaxInstanceCount)
		return E_FAIL;

	_matrix ViewInverse = XMMatrixInverse(nullptr, ViewMatrix);

	_vector vRight = XMVector3Normalize(ViewInverse.r[0]) * vSize.x;
	_vector vUp = XMVector3Normalize(ViewInverse.r[1]) * vSize.y;
	_vector vLook = XMVector3Normalize(ViewInverse.r[2]);

	const _uint iNumInstances = static_cast<_uint>(Positions.size());

	for (_uint i = 0; i < iNumInstances; ++i)
	{
		XMStoreFloat4(&m_Instances[i].vRight, vRight);
		XMStoreFloat4(&m_Instances[i].vUp, vUp);
		XMStoreFloat4(&m_Instances[i].vLook, vLook);
		m_Instances[i].vTranslation = Positions[i];
		m_Instances[i].vTexInfo = vTexInfo;
		m_Instances[i].vColor = vColor;
	}

	return Update_InstanceBuffer(m_Instances.data(), iNumInstances, sizeof(VTXRECT_INSTANCE));
}

CVIBuffer_Rect_Instance* CVIBuffer_Rect_Instance::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, _uint iMaxInstanceCount)
{
	CVIBuffer_Rect_Instance* pInstance = new CVIBuffer_Rect_Instance(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype(iMaxInstanceCount)))
	{
		MSG_BOX("Failed to Created : CVIBuffer_Rect_Instance");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CVIBuffer_Rect_Instance::Clone(void* pArg)
{
	CVIBuffer_Rect_Instance* pInstance = new CVIBuffer_Rect_Instance(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CVIBuffer_Rect_Instance");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CVIBuffer_Rect_Instance::Free()
{
	__super::Free();
}
