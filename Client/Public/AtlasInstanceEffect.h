#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class CShader;
class CTexture;
class CVIBuffer_Rect_Instance;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CAtlasInstanceEffect final : public CGameObject
{
public:
	typedef struct tagAtlasInstanceEffectDesc : public CGameObject::GAMEOBJECT_DESC
	{
		const _tchar*			pTexturePrototypeTag = { nullptr };
		const vector<_float4>*	pPositions = { nullptr };
		_uint					iMaxInstanceCount = { 128 };
		_uint					iAtlasCols = { 1 };
		_uint					iAtlasRows = { 1 };
		_float					fFrameDuration = { 0.08f };
		_float2					vSize = { 1.f, 1.f };
		_float2					vUVPadding = { 0.f, 0.f };
		_float4					vColor = { 1.f, 1.f, 1.f, 1.f };
		_float					fAlpha = { 1.f };
		_bool					bLoop = { true };
	}ATLAS_INSTANCE_EFFECT_DESC;

private:
	CAtlasInstanceEffect(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CAtlasInstanceEffect(const CAtlasInstanceEffect& Prototype);
	virtual ~CAtlasInstanceEffect() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Update(_float fTimeDelta) override;
	virtual void Late_Update(_float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	HRESULT Ready_Components(const ATLAS_INSTANCE_EFFECT_DESC* pDesc);
	HRESULT Bind_ShaderResources();
	_float4 Compute_TexInfo() const;
	void Advance_Frame(_float fTimeDelta);

private:
	CShader*						m_pShaderCom = { nullptr };
	CTexture*						m_pTextureCom = { nullptr };
	CVIBuffer_Rect_Instance*		m_pVIBufferInstanceCom = { nullptr };

	vector<_float4>				m_Positions;
	_wstring						m_strTexturePrototypeTag;

	_uint							m_iAtlasCols = { 1 };
	_uint							m_iAtlasRows = { 1 };
	_uint							m_iCurFrame = {};
	_float							m_fFrameTimer = {};
	_float							m_fFrameDuration = { 0.08f };
	_float2							m_vSize = { 1.f, 1.f };
	_float2							m_vUVPadding = { 0.f, 0.f };
	_float4							m_vColor = { 1.f, 1.f, 1.f, 1.f };
	_float							m_fAlpha = { 1.f };
	_bool							m_bLoop = { true };
	_bool							m_bFinished = {};

public:
	static CAtlasInstanceEffect* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject* Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
