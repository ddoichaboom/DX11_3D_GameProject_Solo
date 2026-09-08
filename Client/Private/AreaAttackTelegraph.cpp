#include "AreaAttackTelegraph.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

CAreaAttackTelegraph::CAreaAttackTelegraph(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CAreaAttackTelegraph::CAreaAttackTelegraph(const CAreaAttackTelegraph& Prototype)
	: CGameObject{ Prototype }
{
}

HRESULT CAreaAttackTelegraph::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CAreaAttackTelegraph::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	AREA_ATTACK_TELEGRAPH_DESC* pDesc = static_cast<AREA_ATTACK_TELEGRAPH_DESC*>(pArg);
	if (nullptr == pDesc->pTexturePrototypeTag)
		return E_FAIL;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (nullptr != pDesc->pObjectName)
		Set_Name(pDesc->pObjectName);

	m_strTexturePrototypeTag = pDesc->pTexturePrototypeTag;
	m_vColor = pDesc->vColor;
	m_fYOffset = pDesc->fYOffset;
	m_bActive = pDesc->bActive;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void CAreaAttackTelegraph::Update(_float fTimeDelta)
{
	if (false == m_bActive)
		return;

	m_fElapsed += fTimeDelta;
	m_fFillRatio = min(1.f, m_fElapsed / max(0.001f, m_fDuration));
}

void CAreaAttackTelegraph::Late_Update(_float fTimeDelta)
{
	if (false == m_bActive)
		return;

	m_pGameInstance->Add_RenderGroup(RENDERID::BLEND, this);
}

HRESULT CAreaAttackTelegraph::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Begin(8)))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Render()))
		return E_FAIL;

	return S_OK;
}

void CAreaAttackTelegraph::Play(const AREA_ATTACK_DESC& Desc, const _float3& vCenter, _float fDuration)
{
	m_Desc = Desc;
	m_vCenter = vCenter;
	m_fDuration = max(0.001f, fDuration);
	m_fElapsed = 0.f;
	m_fFillRatio = 0.f;
	m_fInnerRatio = (m_Desc.fOuterRadius > 0.f) ? (m_Desc.fInnerRadius / m_Desc.fOuterRadius) : 0.f;
	m_fInnerRatio = max(0.f, min(m_fInnerRatio, 0.98f));
	m_bActive = true;

	Update_WorldMatrix();
}

void CAreaAttackTelegraph::Stop()
{
	m_bActive = false;
	m_fElapsed = 0.f;
	m_fFillRatio = 0.f;
}

HRESULT CAreaAttackTelegraph::Ready_Components()
{
	if (FAILED(Add_Component(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_Shader_VtxTex"),
		TEXT("Com_Shader"),
		reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		m_strTexturePrototypeTag,
		TEXT("Com_Texture"),
		reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	if (FAILED(Add_Component(ETOUI(LEVEL::STATIC),
		TEXT("Prototype_Component_VIBuffer_Rect"),
		TEXT("Com_VIBuffer"),
		reinterpret_cast<CComponent**>(&m_pVIBufferCom))))
		return E_FAIL;

	return S_OK;
}

HRESULT CAreaAttackTelegraph::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_vColor", &m_vColor, sizeof(_float4))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fAreaInnerRatio", &m_fInnerRatio, sizeof(_float))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fAreaFillRatio", &m_fFillRatio, sizeof(_float))))
		return E_FAIL;

	return S_OK;
}

void CAreaAttackTelegraph::Update_WorldMatrix()
{
	const _float fDiameter = max(0.001f, m_Desc.fOuterRadius * 2.f);

	_float4x4 World{};
	XMStoreFloat4x4(&World, XMMatrixIdentity());

	World.m[0][0] = fDiameter;
	World.m[1][1] = 0.f;
	World.m[1][2] = fDiameter;
	World.m[2][1] = 1.f;
	World.m[2][2] = 0.f;
	World.m[3][0] = m_vCenter.x;
	World.m[3][1] = m_vCenter.y + m_fYOffset;
	World.m[3][2] = m_vCenter.z;
	World.m[3][3] = 1.f;

	m_pTransformCom->Set_WorldMatrix(World);
}

CAreaAttackTelegraph* CAreaAttackTelegraph::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CAreaAttackTelegraph* pInstance = new CAreaAttackTelegraph(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CAreaAttackTelegraph");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CAreaAttackTelegraph::Clone(void* pArg)
{
	CAreaAttackTelegraph* pInstance = new CAreaAttackTelegraph(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CAreaAttackTelegraph");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CAreaAttackTelegraph::Free()
{
	__super::Free();

	Safe_Release(m_pVIBufferCom);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pShaderCom);
}
