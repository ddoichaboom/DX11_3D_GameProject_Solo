#include "VIBuffer_Instance.h"

CVIBuffer_Instance::CVIBuffer_Instance(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CVIBuffer{ pDevice, pContext }
{
}

CVIBuffer_Instance::CVIBuffer_Instance(const CVIBuffer_Instance& Prototype)
	: CVIBuffer{ Prototype }
	, m_iNumInstances{ Prototype.m_iNumInstances }
	, m_iInstanceCapacity{ Prototype.m_iInstanceCapacity }
	, m_iInstanceStride{ Prototype.m_iInstanceStride }
	, m_iIndexCountPerInstance{ Prototype.m_iIndexCountPerInstance }
	, m_InstanceBufferDesc{ Prototype.m_InstanceBufferDesc }
{
}

HRESULT CVIBuffer_Instance::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CVIBuffer_Instance::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CVIBuffer_Instance::Bind_Resources()
{
	ID3D11Buffer* pVertexBuffers[] = {
		m_pVB,
		m_pVBInstance
	};

	_uint iVertexStrides[] = {
		m_iVertexStride,
		m_iInstanceStride
	};

	_uint iOffsets[] = {
		0,
		0
	};

	m_pContext->IASetVertexBuffers(0, m_iNumVertexBuffers, pVertexBuffers, iVertexStrides, iOffsets);
	m_pContext->IASetIndexBuffer(m_pIB, m_eIndexFormat, 0);
	m_pContext->IASetPrimitiveTopology(m_ePrimitiveType);

	return S_OK;
}

HRESULT CVIBuffer_Instance::Render()
{
	if (0 == m_iNumInstances)
		return S_OK;

	m_pContext->DrawIndexedInstanced(m_iIndexCountPerInstance, m_iNumInstances, 0, 0, 0);

	return S_OK;
}

HRESULT CVIBuffer_Instance::Create_InstanceBuffer(const void* pInitialData, _uint iInstanceCapacity, _uint iInstanceStride)
{
	if (0 == iInstanceCapacity || 0 == iInstanceStride)
		return E_FAIL;

	m_iInstanceCapacity = iInstanceCapacity;
	m_iInstanceStride = iInstanceStride;

	m_InstanceBufferDesc = {};
	m_InstanceBufferDesc.ByteWidth = m_iInstanceCapacity * m_iInstanceStride;
	m_InstanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	m_InstanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	m_InstanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_InstanceBufferDesc.MiscFlags = 0;
	m_InstanceBufferDesc.StructureByteStride = m_iInstanceStride;

	D3D11_SUBRESOURCE_DATA InstanceInitialData{};
	InstanceInitialData.pSysMem = pInitialData;

	Safe_Release(m_pVBInstance);

	if (FAILED(m_pDevice->CreateBuffer(&m_InstanceBufferDesc, pInitialData ? &InstanceInitialData : nullptr, &m_pVBInstance)))
		return E_FAIL;

	return S_OK;
}

HRESULT CVIBuffer_Instance::Update_InstanceBuffer(const void* pData, _uint iNumInstances, _uint iInstanceStride)
{
	if (nullptr == pData || nullptr == m_pVBInstance)
		return E_FAIL;

	if (iNumInstances > m_iInstanceCapacity || iInstanceStride != m_iInstanceStride)
		return E_FAIL;

	D3D11_MAPPED_SUBRESOURCE SubResource{};

	if (FAILED(m_pContext->Map(m_pVBInstance, 0, D3D11_MAP_WRITE_DISCARD, 0, &SubResource)))
		return E_FAIL;

	memcpy(SubResource.pData, pData, iNumInstances * iInstanceStride);

	m_pContext->Unmap(m_pVBInstance, 0);

	m_iNumInstances = iNumInstances;

	return S_OK;
}

void CVIBuffer_Instance::Free()
{
	__super::Free();

	Safe_Release(m_pVBInstance);
}
