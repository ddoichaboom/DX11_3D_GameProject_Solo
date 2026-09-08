#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class CShader;
class CTexture;
class CVIBuffer_Rect;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CAreaAttackTelegraph final : public CGameObject
{
public:
	typedef struct tagAreaAttackTelegraphDesc : public CGameObject::GAMEOBJECT_DESC
	{
		const _tchar* pObjectName = { nullptr };
		const _tchar* pTexturePrototypeTag = { nullptr };
		_float4 vColor = { 0.85f, 0.02f, 0.04f, 0.55f };
		_float fYOffset = { 0.05f };
		_bool bActive = { false };
	} AREA_ATTACK_TELEGRAPH_DESC;

private:
	CAreaAttackTelegraph(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CAreaAttackTelegraph(const CAreaAttackTelegraph& Prototype);
	virtual ~CAreaAttackTelegraph() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

public:
	void Play(const AREA_ATTACK_DESC& Desc, const _float3& vCenter, _float fDuration);
	void Stop();

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();
	void Update_WorldMatrix();

private:
	CShader* m_pShaderCom = { nullptr };
	CTexture* m_pTextureCom = { nullptr };
	CVIBuffer_Rect* m_pVIBufferCom = { nullptr };

	_wstring m_strTexturePrototypeTag;
	AREA_ATTACK_DESC m_Desc = {};
	_float3 m_vCenter = {};
	_float4 m_vColor = { 0.85f, 0.02f, 0.04f, 0.55f };
	_float m_fElapsed = { 0.f };
	_float m_fDuration = { 0.45f };
	_float m_fFillRatio = { 0.f };
	_float m_fInnerRatio = { 0.f };
	_float m_fYOffset = { 0.05f };
	_bool m_bActive = { false };

public:
	static CAreaAttackTelegraph* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
