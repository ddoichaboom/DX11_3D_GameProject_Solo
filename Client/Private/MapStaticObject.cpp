#include "MapStaticObject.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Model.h"
#include "PartObject.h"

CMapStaticObject::CMapStaticObject(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPartObject { pDevice, pContext}
{
}

CMapStaticObject::CMapStaticObject(const CMapStaticObject& Prototype)
	: CPartObject{ Prototype }
{
}

HRESULT CMapStaticObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CMapStaticObject::Initialize(void* pArg)
{
	auto	pDesc  = static_cast<MAPSTATICOBJECT_DESC*>(pArg);

	m_strShaderProtoTag = pDesc->pShaderProtoTag;
	m_strModelProtoTag = pDesc->pModelProtoTag;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CMapStaticObject::Priority_Update(_float fTimeDelta)
{
}

void CMapStaticObject::Update(_float fTimeDelta)
{
}

void CMapStaticObject::Late_Update(_float fTimeDelta)
{
	__super::Compute_CombinedWorldMatrix(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()));

	m_pGameInstance->Add_RenderGroup(RENDERID::NONBLEND, this);
}

HRESULT CMapStaticObject::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	_uint iNumMeshes = m_pModelCom->Get_NumMeshes();

	m_pGameInstance->Transform_Frustum_ToLocalSpace(XMLoadFloat4x4(&m_CombinedWorldMatrix));

	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		_float3 vLocalCenter = {};
		_float3 vLocalHalf = {};

		if (true == m_pModelCom->Get_MeshLocalAABB(i, vLocalCenter, vLocalHalf))
		{
			const _float fRadius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&vLocalHalf)));

			if (false == m_pGameInstance->Is_In_Frustum_LocalSpace(
				XMVectorSetW(XMLoadFloat3(&vLocalCenter), 1.f),
				fRadius))
			{
				continue;
			}
		}

		if (FAILED(m_pModelCom->Bind_Material(m_pShaderCom, "g_DiffuseTexture", i, TEXTURE_TYPE::DIFFUSE)))
			continue;

		_float fHasSpec = SUCCEEDED(m_pModelCom->Bind_Material(m_pShaderCom, "g_SpecularTexture", i, TEXTURE_TYPE::SPECULAR)) ? 1.f : 0.f;
		m_pShaderCom->Bind_RawValue("g_fHasSpecularMap", &fHasSpec, sizeof(_float));

		if (FAILED(m_pShaderCom->Begin(1)))
			return E_FAIL;

		if (FAILED(m_pModelCom->Render(i)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT	CMapStaticObject::Ready_Components()
{
	if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
		m_strShaderProtoTag.c_str(),
		TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
		m_strModelProtoTag.c_str(),
		TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
		return E_FAIL;

	return S_OK;
}

HRESULT	CMapStaticObject::Bind_ShaderResources()
{
	if (FAILED(m_pShaderCom->Bind_Matrix("g_WorldMatrix", &m_CombinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	return S_OK;
}


CMapStaticObject* CMapStaticObject::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CMapStaticObject* pInstance = new CMapStaticObject(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CMapStaticObject");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CMapStaticObject::Clone(void* pArg)
{
	CMapStaticObject* pInstance = new CMapStaticObject(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CMapStaticObject");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CMapStaticObject::Free()
{
	__super::Free();

	Safe_Release(m_pShaderCom);
	Safe_Release(m_pModelCom);
}
