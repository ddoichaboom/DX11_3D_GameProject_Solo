#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)

class  ENGINE_DLL CCamera abstract : public CGameObject
{
public:
	typedef struct tagCameraDesc : public CGameObject::GAMEOBJECT_DESC
	{
		_float3			vEye{}, vAt{};
		_float			fFovy{}, fNear{}, fFar{};
	}CAMERA_DESC;

protected:
	CCamera(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CCamera(const CCamera& Prototype);
	virtual ~CCamera() = default;

public:
	virtual HRESULT			Initialize_Prototype();
	virtual HRESULT			Initialize(void* pArg);
	virtual void			Priority_Update(_float fTimeDelta);
	virtual void			Update(_float fTimeDelta);
	virtual void			Late_Update(_float fTimeDelta);
	virtual HRESULT			Render();

public:
	void					Update_PipeLine();		
	void					Resize_Projection(_uint iWidth, _uint iHeight);
	void					Set_ActiveCamera(_bool bActive) { m_bActiveCamera = bActive; }
	_bool					Is_ActiveCamera() const { return m_bActiveCamera; }

protected:
	class CPipeLine*		m_pPipeLine = { nullptr };
	
protected:
	_float					m_fFovy{}, m_fAspect{}, m_fNear{}, m_fFar{};
	_float4x4				m_ProjMatrix = {};

	_bool					m_bActiveCamera = { true };


public:
	virtual CGameObject*	Clone(void* pArg) PURE;
	virtual void			Free() override;

};

NS_END