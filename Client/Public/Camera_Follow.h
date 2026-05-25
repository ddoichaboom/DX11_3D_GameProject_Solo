#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Engine)
class CSpringArm;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CCamera_Follow final : public CCamera
{
public:
	typedef struct tagCameraFollowDesc final : public CCamera::CAMERA_DESC
	{
		_wstring			strTargetLayerTag = TEXT("Layer_Player");
		const _float4x4*	pTargetWorldMatrix = { nullptr };
		_float3				vHeightOffset = { 0.f, 1.5f, 0.f };
		_float				fIdealDistance     = { 5.f };		
		_float				fInitialYaw = {0.f};
		_float				fInitialPitch = {-0.3f};
		_float				fPitchMin = {-1.1f};
		_float				fPitchMax = {1.0f};
		_float				fMouseSensor = {0.003f};
		_float				fArmLerpSpeed = {8.f};
		const vector<_float3>* pCamColliderVertices = { nullptr };
		const vector<CAMCOLLIDER_FACE>* pCamColliderFaces = { nullptr };
	}CAMERA_FOLLOW_DESC;

private:
	CCamera_Follow(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CCamera_Follow(const CCamera_Follow& Prototype);
	virtual ~CCamera_Follow() = default;

public:
	virtual HRESULT			Initialize_Prototype() override;
	virtual HRESULT			Initialize(void* pArg) override;
	virtual void			Priority_Update(_float fTimeDelta) override;
	virtual void			Update(_float fTimeDelta) override;
	virtual void			Late_Update(_float fTimeDelta) override;
	virtual HRESULT			Render() override;

public:
	_float					Get_Yaw() const;

private:
	CSpringArm*				m_pSpringArm = { nullptr };

	_wstring				m_strTargetLayerTag = TEXT("Layer_Player");
	vector<_float3>			m_CamColliderVertices;
	vector<CAMCOLLIDER_FACE> m_CamColliderFaces;
	_float					m_fCamColliderPadding = { 0.25f };

private:
	HRESULT					Ready_Components(const CAMERA_FOLLOW_DESC& Desc);
	void					Apply_CamCollider();
	void					Apply_SpringArmToTransform();
	void					Rebind_Target();
	const _float4x4*		Find_TargetWorldMatrix() const;

public:
	static	CCamera_Follow* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject*	Clone(void* pArg) override;
	virtual void			Free() override;
};

NS_END
