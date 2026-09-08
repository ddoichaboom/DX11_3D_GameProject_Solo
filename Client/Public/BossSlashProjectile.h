#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class CShader;
class CTexture;
class CVIBuffer_Rect;
class CCollider;
NS_END

NS_BEGIN(Client)

class CMonster;

class CLIENT_DLL CBossSlashProjectile final : public CGameObject
{
public:
	typedef struct tagBossSlashProjectileDesc : public CGameObject::GAMEOBJECT_DESC
	{
		CMonster* pOwnerMonster = { nullptr };
		const _tchar* pTexturePrototypeTag = { TEXT("Prototype_Component_Texture_Effect_Slash_IgrisProjectile") };
		_float3 vStartPosition = {};
		_float3 vDirection = { 0.f, 0.f, 1.f };
		_float2 vSize = { 3.6f, 1.4f };
		_float fSpeed = { 32.f };
		_float fLifeTime = { 1.2f };
		_float fDamage = { 400.f };
		_float fColliderRadius = { 1.6f };
		_bool bPierce = { false };
	}BOSS_SLASH_PROJECTILE_DESC;

private:
	CBossSlashProjectile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CBossSlashProjectile(const CBossSlashProjectile& Prototype);
	virtual ~CBossSlashProjectile() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	HRESULT Ready_Components();
	HRESULT Ready_Collider();
	HRESULT Bind_ShaderResources();
	void Update_WorldMatrix();
	void Update_Collider();
	void On_ColliderHit(CCollider* pOther);
	void Deactivate();

private:
	CShader* m_pShaderCom = { nullptr };
	CTexture* m_pTextureCom = { nullptr };
	CVIBuffer_Rect* m_pVIBufferCom = { nullptr };
	CCollider* m_pCollider = { nullptr };

	CMonster* m_pOwnerMonster = { nullptr };
	_wstring m_strTexturePrototypeTag;
	set<CGameObject*> m_HitTargets;

	_float3 m_vPosition = {};
	_float3 m_vDirection = { 0.f, 0.f, 1.f };
	_float2 m_vSize = { 3.6f, 1.4f };
	_float m_fSpeed = { 32.f };
	_float m_fLifeTime = { 1.2f };
	_float m_fElapsed = {};
	_float m_fDamage = { 400.f };
	_float m_fColliderRadius = { 1.6f };
	_float m_fAlpha = { 1.f };
	_float4 m_vTint = { 0.82f, 0.1f, 1.f, 0.85f };
	_bool m_bPierce = { false };
	_bool m_bActive = { false };

public:
	static CBossSlashProjectile* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
