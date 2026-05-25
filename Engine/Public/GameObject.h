#pragma once

#include "Transform.h"

NS_BEGIN(Engine)
class CVIBuffer;

class ENGINE_DLL CGameObject abstract : public CBase 
{
public:
	typedef struct tagGameObjectDesc : public CTransform::TRANSFORM_DESC
	{
		_uint			iFlag			= {};
		TRANSFORMTYPE	eTransformType	= { TRANSFORMTYPE::TRANSFORM_3D };
	}GAMEOBJECT_DESC;

protected:
	CGameObject(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CGameObject(const CGameObject& Prototype);
	virtual ~CGameObject() = default;

public:
	const map<const _wstring, CComponent*>& Get_Components() const { return m_Components; }
	CTransform*								Get_Transform() const { return m_pTransformCom;	}

	CVIBuffer*								Get_VIBuffer() const { return m_pVIBufferCom; }

	const _wstring&							Get_Name() const { return m_strName; }
	void									Set_Name(const _wstring& strName) { m_strName = strName; }
	const _wstring&							Get_Tag() const { return m_strTag; }
	void									Set_Tag(const _wstring& strTag) { m_strTag = strTag; }
	const _wstring&							Get_PrototypeTag() const { return m_strPrototypeTag; }
	void									Set_PrototypeTag(const _wstring& strTag) { m_strPrototypeTag = strTag; }

public:
	virtual HRESULT							Initialize_Prototype();
	virtual HRESULT							Initialize(void* pArg);
	virtual void							Priority_Update(_float fTimeDelta);
	virtual void							Update(_float fTimeDelta);
	virtual void							Late_Update(_float fTimeDelta);
	virtual HRESULT							Render();
	virtual HRESULT							Render_Shadow() { return S_OK; }

protected:
	ID3D11Device*							m_pDevice = { nullptr };
	ID3D11DeviceContext*					m_pContext = { nullptr };
	class CGameInstance*					m_pGameInstance = { nullptr };

	map<const _wstring, CComponent*>		m_Components;
	CTransform*								m_pTransformCom = { nullptr };
	_uint									m_iFlag = {};

	_wstring								m_strName;
	_wstring								m_strTag;
	_wstring								m_strPrototypeTag;

	CVIBuffer*								m_pVIBufferCom = { nullptr };

protected:
	HRESULT									Add_Component(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag,
														const _wstring& strComponentTag, CComponent** ppOut, void* pArg = nullptr);
	CComponent*								Find_Component(const _wstring& strComponentTag);

public:
	// Create
	virtual CGameObject*					Clone(void* pArg) PURE;
	virtual void							Free() override;

};

NS_END