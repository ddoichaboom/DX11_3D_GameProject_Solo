#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class CMesh final : public CVIBuffer
{
private:
	CMesh(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CMesh(const CMesh& Prototype);
	virtual ~CMesh() = default;

public:
	_bool					Get_LocalAABB(_float3& vOutCenter, _float3& vOutHalfExtent) const;

	_uint					Get_MaterialIndex() const { return m_iMaterialIndex; }
	const _char*			Get_Name() const {	return m_szName; }

	// CPU 스키닝 구현 도전
	const vector<XMUINT4>&		Get_PickBlendIndices() const { return m_PickBlendIndices; }
	const vector<XMFLOAT4>&		Get_PickBlendWeights() const { return m_PickBlendWeights; }
	const vector<_uint>&		Get_BoneIndices() const { return m_BoneIndices; }
	const vector<_float4x4>&	Get_OffsetMatrices() const { return m_OffsetMatrices; }
	_uint						Get_NumBones() const { return m_iNumBones; }

public:
	virtual HRESULT			Initialize_Prototype(MODEL eType, const MESH_DESC& Desc);
	virtual HRESULT			Initialize(void* pArg) override;

public:
	HRESULT					Bind_BoneMatrices(class CShader* pShader, const _char* pConstantName, const vector<class CBone*>& Bones);

private:
	HRESULT					Ready_NonAnimMesh(const MESH_DESC& Desc);
	HRESULT					Ready_AnimMesh(const MESH_DESC& Desc);

	void					Build_LocalAABB();

private:
	_char					m_szName[MAX_PATH] = {};
	_uint					m_iMaterialIndex = {};

	_uint					m_iNumBones = {};
	vector<_uint>			m_BoneIndices = {};
	vector<_float4x4>		m_OffsetMatrices;
	_float4x4				m_BoneMatrices[g_iNumMeshBones] = {};

	_float3					m_vLocalAABBCenter = {};
	_float3					m_vLocalAABBHalfExtent = {};
	_bool					m_bLocalAABBValid = { false };

	// CPU 스키닝 - PICK_DATA 변환해서 PICKING 하기 위함 
	vector<XMUINT4>			m_PickBlendIndices;
	vector<XMFLOAT4>		m_PickBlendWeights;

public:
	static CMesh*			Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, MODEL eType, const MESH_DESC& Desc);
	virtual CComponent*		Clone(void* pArg) override;
	virtual void			Free() override;
};

NS_END