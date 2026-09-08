#include "BossSlashProjectile.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include "Collider.h"
#include "Monster.h"
#include "Player.h"

CBossSlashProjectile::CBossSlashProjectile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CBossSlashProjectile::CBossSlashProjectile(const CBossSlashProjectile& Prototype)
	: CGameObject{ Prototype }
{
}

HRESULT CBossSlashProjectile::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CBossSlashProjectile::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	BOSS_SLASH_PROJECTILE_DESC* pDesc = static_cast<BOSS_SLASH_PROJECTILE_DESC*>(pArg);
	if (nullptr == pDesc->pTexturePrototypeTag)
		return E_FAIL;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pOwnerMonster = pDesc->pOwnerMonster;
	m_strTexturePrototypeTag = pDesc->pTexturePrototypeTag;
	m_vPosition = pDesc->vStartPosition;
	m_vDirection = pDesc->vDirection;
	m_vSize = pDesc->vSize;
	m_fSpeed = pDesc->fSpeed;
	m_fLifeTime = max(0.001f, pDesc->fLifeTime);
	m_fDamage = pDesc->fDamage;
	m_fColliderRadius = pDesc->fColliderRadius;
	m_bPierce = pDesc->bPierce;
	m_bActive = true;

	_vector vDir = XMLoadFloat3(&m_vDirection);
	vDir = XMVectorSetY(vDir, 0.f);
	if (XMVectorGetX(XMVector3LengthSq(vDir)) <= 0.0001f)
		vDir = XMVectorSet(0.f, 0.f, 1.f, 0.f);

	XMStoreFloat3(&m_vDirection, XMVector3Normalize(vDir));

	if (FAILED(Ready_Components()))
		return E_FAIL;

	if (FAILED(Ready_Collider()))
		return E_FAIL;

	Update_WorldMatrix();

	return S_OK;
}

void CBossSlashProjectile::Update(_float fTimeDelta)
{
	if (false == m_bActive)
		return;

	m_fElapsed += fTimeDelta;
	if (m_fElapsed >= m_fLifeTime)
	{
		Deactivate();
		return;
	}

	const _float fMove = m_fSpeed * fTimeDelta;
	m_vPosition.x += m_vDirection.x * fMove;
	m_vPosition.y += m_vDirection.y * fMove;
	m_vPosition.z += m_vDirection.z * fMove;
	m_fAlpha = max(0.f, 1.f - (m_fElapsed / m_fLifeTime));

	Update_WorldMatrix();
}

void CBossSlashProjectile::Late_Update(_float fTimeDelta)
{
	if (false == m_bActive)
		return;

	Update_Collider();
	m_pGameInstance->Add_RenderGroup(RENDERID::BLEND, this);
}

HRESULT CBossSlashProjectile::Render()
{
	if (false == m_bActive)
		return S_OK;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Begin(9)))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pVIBufferCom->Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT CBossSlashProjectile::Ready_Components()
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

HRESULT CBossSlashProjectile::Ready_Collider()
{
	m_pCollider = CCollider::Create(m_pDevice, m_pContext);
	if (nullptr == m_pCollider)
		return E_FAIL;

	CCollider::COLLIDER_DESC Desc{};
	Desc.eBoundingType = COLLIDER::SPHERE;
	Desc.eGroup = COLLISION_GROUP::MONSTER_ATTACK;
	Desc.vCenter = _float3(0.f, 0.f, 0.f);
	Desc.vSize = _float3(m_fColliderRadius, 0.f, 0.f);
	Desc.fRadius = m_fColliderRadius;
	Desc.pOwner = this;

	if (FAILED(m_pCollider->Initialize(&Desc)))
	{
		Safe_Release(m_pCollider);
		return E_FAIL;
	}

	m_pCollider->Set_OnHitEnter([this](CCollider* pOther)
		{
			On_ColliderHit(pOther);
		});

	m_pCollider->Set_OnHitStay([this](CCollider* pOther)
		{
			On_ColliderHit(pOther);
		});

	return S_OK;
}

HRESULT CBossSlashProjectile::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
		return E_FAIL;

	_float4 vTint = m_vTint;
	vTint.w *= m_fAlpha;
	if (FAILED(m_pShaderCom->Bind_RawValue("g_vSweepTint", &vTint, sizeof(_float4))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &m_fAlpha, sizeof(_float))))
		return E_FAIL;

	return S_OK;
}

void CBossSlashProjectile::Update_WorldMatrix()
{
	_vector vLook = XMLoadFloat3(&m_vDirection);
	vLook = XMVector3Normalize(XMVectorSetY(vLook, 0.f));

	_vector vUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector vToCamera = XMLoadFloat4(m_pGameInstance->Get_CamPosition()) -
		XMVectorSet(m_vPosition.x, m_vPosition.y, m_vPosition.z, 1.f);
	vToCamera = XMVectorSetY(vToCamera, 0.f);

	if (XMVectorGetX(XMVector3LengthSq(vToCamera)) <= 0.0001f)
		vToCamera = vLook;
	else
		vToCamera = XMVector3Normalize(vToCamera);

	_vector vRight = XMVector3Normalize(XMVector3Cross(vUp, vToCamera));
	if (XMVectorGetX(XMVector3LengthSq(vRight)) <= 0.0001f)
		vRight = XMVectorSet(1.f, 0.f, 0.f, 0.f);

	vUp = XMVector3Normalize(XMVector3Cross(vToCamera, vRight));

	_float4x4 World{};
	XMStoreFloat4x4(&World, XMMatrixIdentity());

	XMStoreFloat4(reinterpret_cast<_float4*>(&World.m[ETOUI(STATE::RIGHT)]), XMVectorScale(vRight, m_vSize.x));
	XMStoreFloat4(reinterpret_cast<_float4*>(&World.m[ETOUI(STATE::UP)]), XMVectorScale(vUp, m_vSize.y));
	XMStoreFloat4(reinterpret_cast<_float4*>(&World.m[ETOUI(STATE::LOOK)]), XMVectorScale(vToCamera, 0.01f));
	World.m[3][0] = m_vPosition.x;
	World.m[3][1] = m_vPosition.y;
	World.m[3][2] = m_vPosition.z;
	World.m[3][3] = 1.f;

	m_pTransformCom->Set_WorldMatrix(World);
}

void CBossSlashProjectile::Update_Collider()
{
	if (nullptr == m_pCollider || false == m_bActive)
		return;

	_matrix World = XMMatrixIdentity();
	World.r[3] = XMVectorSet(m_vPosition.x, m_vPosition.y, m_vPosition.z, 1.f);

	m_pCollider->Update(World);
	m_pCollider->Register();
}

void CBossSlashProjectile::On_ColliderHit(CCollider* pOther)
{
	if (false == m_bActive || nullptr == pOther)
		return;

	if (COLLISION_GROUP::PLAYER_BODY != pOther->Get_Group())
		return;

	CGameObject* pOwner = pOther->Get_Owner();
	if (nullptr == pOwner)
		return;

	if (m_HitTargets.end() != m_HitTargets.find(pOwner))
		return;

	m_HitTargets.insert(pOwner);

	CPlayer* pPlayer = dynamic_cast<CPlayer*>(pOwner);
	if (nullptr != pPlayer)
		pPlayer->Take_Damage(m_fDamage, m_pOwnerMonster);

	if (false == m_bPierce)
		Deactivate();
}

void CBossSlashProjectile::Deactivate()
{
	m_bActive = false;
	m_HitTargets.clear();
}

CBossSlashProjectile* CBossSlashProjectile::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CBossSlashProjectile* pInstance = new CBossSlashProjectile(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CBossSlashProjectile");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CBossSlashProjectile::Clone(void* pArg)
{
	CBossSlashProjectile* pInstance = new CBossSlashProjectile(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CBossSlashProjectile");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CBossSlashProjectile::Free()
{
	if (nullptr != m_pCollider)
		m_pCollider->Clear_Callbacks();

	Safe_Release(m_pCollider);
	Safe_Release(m_pVIBufferCom);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pShaderCom);

	__super::Free();
}
