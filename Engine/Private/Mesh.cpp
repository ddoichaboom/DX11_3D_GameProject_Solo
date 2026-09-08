#include "Mesh.h"
#include "Bone.h"
#include "Shader.h"

CMesh::CMesh(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CVIBuffer { pDevice, pContext }
{
}

CMesh::CMesh(const CMesh& Prototype)
	: CVIBuffer{ Prototype }
{
}

_bool CMesh::Get_LocalAABB(_float3& vOutCenter, _float3& vOutHalfExtent) const
{
	if (false == m_bLocalAABBValid)
		return false;

	vOutCenter = m_vLocalAABBCenter;
	vOutHalfExtent = m_vLocalAABBHalfExtent;
	return true;
}

HRESULT CMesh::Initialize_Prototype(MODEL eType, const MESH_DESC& Desc)
{
	strcpy_s(m_szName, Desc.szName);
	m_iMaterialIndex = Desc.iMaterialIndex;

	m_iNumVertices = Desc.iNumVertices;
	m_iNumIndices = Desc.iNumIndices;
	m_iVertexStride = Desc.iVertexStride;

	m_iIndexStride = sizeof(_uint);
	m_eIndexFormat = DXGI_FORMAT_R32_UINT;
	m_iNumVertexBuffers = 1;
	m_ePrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if (MODEL::NONANIM == eType)
	{
		if (FAILED(Ready_NonAnimMesh(Desc)))
			return E_FAIL;
	}
	else
	{
		if (FAILED(Ready_AnimMesh(Desc)))
			return E_FAIL;
	}

	// 인덱스 버퍼(공용)
	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = m_iIndexStride * m_iNumIndices;
	IBDesc.Usage = D3D11_USAGE_DEFAULT;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBInitialData = {};
	IBInitialData.pSysMem = Desc.pIndices;

	if (FAILED(m_pDevice->CreateBuffer(&IBDesc, &IBInitialData, &m_pIB)))
		return E_FAIL;

	// PICK_DATA - 인덱스
	m_pPickData->iNumIndices = m_iNumIndices;
	m_pPickData->pIndices = new _uint[m_iNumIndices];
	memcpy(m_pPickData->pIndices, Desc.pIndices, sizeof(_uint) * m_iNumIndices);

	return S_OK;
}

HRESULT CMesh::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CMesh::Bind_BoneMatrices(CShader* pShader, const _char* pConstantName, const vector<class CBone*>& Bones)
{
	ZeroMemory(m_BoneMatrices, sizeof(_float4x4) * g_iNumMeshBones);

	for (_uint i = 0; i < m_iNumBones; ++i)
	{
		_matrix OffsetMatrix = XMLoadFloat4x4(&m_OffsetMatrices[i]);
		_matrix CombinedMatrix = XMLoadFloat4x4(Bones[m_BoneIndices[i]]->Get_CombinedTransformMatrixPtr());

		XMStoreFloat4x4(&m_BoneMatrices[i], OffsetMatrix * CombinedMatrix);
	}

	return pShader->Bind_Matrices(pConstantName, m_BoneMatrices, m_iNumBones);
}

HRESULT CMesh::Ready_NonAnimMesh(const MESH_DESC& Desc)
{
	// 정점 버퍼 - VTXMESH
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = m_iVertexStride * m_iNumVertices;
	VBDesc.Usage = D3D11_USAGE_DEFAULT;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBInitialData = {};
	VBInitialData.pSysMem = Desc.pVertices;

	if (FAILED(m_pDevice->CreateBuffer(&VBDesc, &VBInitialData, &m_pVB)))
		return E_FAIL;

	// PICK_DATA
	m_pPickData = new PICK_DATA;
	m_pPickData->iNumVertices = m_iNumVertices;
	m_pPickData->pVerticesPos = new _float3[m_iNumVertices];

	VTXMESH* pVtx = static_cast<VTXMESH*>(Desc.pVertices);
	for (_uint i = 0; i < m_iNumVertices; ++i)
		m_pPickData->pVerticesPos[i] = pVtx[i].vPosition;

	Build_LocalAABB();

	return S_OK;
}

HRESULT CMesh::Ready_AnimMesh(const MESH_DESC& Desc)
{
	// 정점 버퍼 - VTXANIMMESH
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = m_iVertexStride * m_iNumVertices;
	VBDesc.Usage = D3D11_USAGE_DEFAULT;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBInitialData = {};
	VBInitialData.pSysMem = Desc.pVertices;

	if (FAILED(m_pDevice->CreateBuffer(&VBDesc, &VBInitialData, &m_pVB)))
		return E_FAIL;

	// 본 데이터 저장
	m_iNumBones = Desc.iNumBones;

	m_BoneIndices.resize(m_iNumBones);
	memcpy(m_BoneIndices.data(), Desc.pBoneIndices, sizeof(_uint) * m_iNumBones);

	m_OffsetMatrices.resize(m_iNumBones);
	memcpy(m_OffsetMatrices.data(), Desc.pOffsetMatrices, sizeof(_float4x4) * m_iNumBones);

	// PICK_DATA
	m_pPickData = new PICK_DATA;
	m_pPickData->iNumVertices = m_iNumVertices;
	m_pPickData->pVerticesPos = new _float3[m_iNumVertices];

	VTXANIMMESH* pVtx = static_cast<VTXANIMMESH*>(Desc.pVertices);
	for (_uint i = 0; i < m_iNumVertices; ++i)
		m_pPickData->pVerticesPos[i] = pVtx[i].vPosition;

	Build_LocalAABB();

	m_PickBlendIndices.resize(m_iNumVertices);
	m_PickBlendWeights.resize(m_iNumVertices);

	for (size_t i = 0; i < m_iNumVertices; i++)
	{
		m_PickBlendIndices[i] = pVtx[i].vBlendIndex;
		m_PickBlendWeights[i] = pVtx[i].vBlendWeight;
	}

	return S_OK;
}

void CMesh::Build_LocalAABB()
{
	if (nullptr == m_pPickData || nullptr == m_pPickData->pVerticesPos || 0 == m_pPickData->iNumVertices)
		return;

	_vector vMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector vMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);

	for (_uint i = 0; i < m_pPickData->iNumVertices; ++i)
	{
		_vector vPos = XMLoadFloat3(&m_pPickData->pVerticesPos[i]);
		vMin = XMVectorMin(vMin, vPos);
		vMax = XMVectorMax(vMax, vPos);
	}

	_vector vCenter = XMVectorScale(XMVectorAdd(vMin, vMax), 0.5f);
	_vector vHalf = XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f);

	XMStoreFloat3(&m_vLocalAABBCenter, vCenter);
	XMStoreFloat3(&m_vLocalAABBHalfExtent, vHalf);

	m_bLocalAABBValid = true;
}

CMesh* CMesh::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, MODEL eType, const MESH_DESC& Desc)
{
	CMesh* pInstance = new CMesh(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype(eType, Desc)))
	{
		MSG_BOX("Failed to Created : CMesh");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CComponent* CMesh::Clone(void* pArg)
{
	return new CMesh(*this);
}

void CMesh::Free()
{
	__super::Free();
}
