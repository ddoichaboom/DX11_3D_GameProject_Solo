#include "NavMeshObject.h"
#include "NavMesh.h"
#include "GameInstance.h"
#include "Shader.h"
#include "VIBuffer_NavMesh.h"

CNavMeshObject::CNavMeshObject(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CNavMeshObject::CNavMeshObject(const CNavMeshObject& Prototype)
	: CGameObject { Prototype }
{
}

HRESULT CNavMeshObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CNavMeshObject::Initialize(void* pArg)
{
	NAVMESHOBJECT_DESC DefaultDesc{};
	NAVMESHOBJECT_DESC* pDesc = (nullptr != pArg)
		? static_cast<NAVMESHOBJECT_DESC*>(pArg)
		: &DefaultDesc;

	m_strName = TEXT("NavMeshObject");

	if (FAILED(__super::Initialize(pDesc)))
		return E_FAIL;

	if (FAILED(Ready_Components(pDesc)))
		return E_FAIL;

	return S_OK;
}

void CNavMeshObject::Priority_Update(_float fTimeDelta)
{
}

void CNavMeshObject::Update(_float fTimeDelta)
{
}

void CNavMeshObject::Late_Update(_float fTimeDelta)
{
	if (nullptr == m_pNavMeshCom)
		return;

	if (0 == m_pNavMeshCom->Get_NumCells())
		return;

	m_pGameInstance->Add_RenderGroup(RENDERID::NONLIGHT, this);
}

HRESULT CNavMeshObject::Render()
{
	if (nullptr == m_pNavMeshCom ||
		nullptr == m_pShaderCom ||
		nullptr == m_pNavMeshBufferCom)
		return S_OK;

	if (0 == m_pNavMeshCom->Get_NumCells())
		return S_OK;

	if (FAILED(m_pNavMeshBufferCom->Build(
		m_pNavMeshCom->Get_Vertices(),
		m_pNavMeshCom->Get_CellDescs())))
		return E_FAIL;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Begin(0)))
		return E_FAIL;

	if (FAILED(m_pNavMeshBufferCom->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pNavMeshBufferCom->Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT CNavMeshObject::Ready_Components(const NAVMESHOBJECT_DESC* pDesc)
{
	CNavMesh::NAVMESH_DESC NavMeshDesc{};
	NavMeshDesc.pInitialSnapshot = (nullptr != pDesc) ? pDesc->pInitialSnapshot : nullptr;

	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_NavMesh"),
		TEXT("Com_NavMesh"),
		reinterpret_cast<CComponent**>(&m_pNavMeshCom),
		&NavMeshDesc)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_Shader_VtxPosColor"),
		TEXT("Com_Shader"),
		reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_VIBuffer_NavMesh"),
		TEXT("Com_VIBuffer_NavMesh"),
		reinterpret_cast<CComponent**>(&m_pNavMeshBufferCom))))
		return E_FAIL;

	return S_OK;
}

HRESULT	CNavMeshObject::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix",
		m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix",
		m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	_float4 vColor = _float4(0.f, 1.f, 0.f, 1.f);
	if (FAILED(m_pShaderCom->Bind_RawValue("g_vColor", &vColor, sizeof(_float4))))
		return E_FAIL;

	return S_OK;
}

CNavMeshObject* CNavMeshObject::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CNavMeshObject* pInstance = new CNavMeshObject(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CNavMeshObject");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CNavMeshObject::Clone(void* pArg)
{
	CNavMeshObject* pInstance = new CNavMeshObject(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CNavMeshObject");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CNavMeshObject::Free()
{
	__super::Free();

	Safe_Release(m_pNavMeshBufferCom);
	Safe_Release(m_pShaderCom);
	Safe_Release(m_pNavMeshCom);
}
