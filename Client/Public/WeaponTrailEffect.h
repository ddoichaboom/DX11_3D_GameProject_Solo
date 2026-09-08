#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class CShader;
class CTexture;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CWeaponTrailEffect final : public CGameObject
{
public:
	struct VTXTRAIL
	{
		_float3 vPosition;
		_float2 vTexcoord;
		_float4 vColor;

		static const unsigned int iNumElements = { 3 };
		static const D3D11_INPUT_ELEMENT_DESC Elements[];
	};

	typedef struct tagWeaponTrailEffectDesc : public CGameObject::GAMEOBJECT_DESC
	{
		const _float4x4* pStartBoneMatrix = { nullptr };
		const _float4x4* pEndBoneMatrix = { nullptr };
		const _float4x4* pParentWorldMatrix = { nullptr };
		const _tchar* pTexturePrototypeTag = { nullptr };
		_float4 vColor = { 1.f, 1.f, 1.f, 1.f };
		_uint iMaxSamples = { 24 };
		_float fSampleInterval = { 0.008f };
		_float fLifeTime = { 0.16f };
		_float fMinSampleDistance = { 0.01f };
		_bool bInitiallyActive = { false };
	}WEAPON_TRAIL_EFFECT_DESC;

private:
	typedef struct tagTrailSample
	{
		_float3 vStart = {};
		_float3 vEnd = {};
		_float fAge = {};
	}TRAIL_SAMPLE;

private:
	CWeaponTrailEffect(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CWeaponTrailEffect(const CWeaponTrailEffect& Prototype);
	virtual ~CWeaponTrailEffect() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

public:
	void Set_Active(_bool bActive);
	void Set_Color(const _float4& vColor) { m_vColor = vColor; }
	void Reset();

private:
	HRESULT Ready_Components();
	HRESULT Ready_Buffers();
	HRESULT Bind_ShaderResources();
	void Sample_CurrentPose();
	_bool Compute_CurrentPoints(_float3* pOutStart, _float3* pOutEnd) const;
	_bool Should_AddSample(const _float3& vStart, const _float3& vEnd) const;
	void Build_Vertices(vector<VTXTRAIL>* pOutVertices) const;

private:
	CShader* m_pShaderCom = { nullptr };
	CTexture* m_pTextureCom = { nullptr };
	ID3D11Buffer* m_pVB = { nullptr };

	const _float4x4* m_pStartBoneMatrix = { nullptr };
	const _float4x4* m_pEndBoneMatrix = { nullptr };
	const _float4x4* m_pParentWorldMatrix = { nullptr };
	_wstring m_strTexturePrototypeTag;

	vector<TRAIL_SAMPLE> m_Samples;
	vector<VTXTRAIL> m_Vertices;

	_uint m_iMaxSamples = { 24 };
	_uint m_iMaxVertices = { 48 };
	_float m_fSampleInterval = { 0.008f };
	_float m_fSampleTimer = {};
	_float m_fLifeTime = { 0.16f };
	_float m_fMinSampleDistance = { 0.01f };
	_float4 m_vColor = { 1.f, 1.f, 1.f, 1.f };
	_bool m_bActive = {};

public:
	static CWeaponTrailEffect* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
