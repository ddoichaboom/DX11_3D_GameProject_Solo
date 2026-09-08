#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Client)

class CLIENT_DLL CCamera_Cinematic final : public CCamera
{
public:
	enum class MODE : _uint
	{
		NONE,
		FIXED,
		ATTACH_PIVOT,
		RAIL,
		PINGPONG,
	};

	typedef struct tagCameraCinematicDesc final : public CCamera::CAMERA_DESC
	{
		_bool	bActive = { false };
	}CAMERA_CINEMATIC_DESC;

	typedef struct tagAnchorDesc final
	{
		CGameObject*	pObject = { nullptr };
		const _tchar*	pPartTag = { nullptr };
		const _char*	pPivotName = { nullptr };
		_float3			vWorldPosition = {};
		_bool			bUseWorldPosition = { false };
	}ANCHOR_DESC;

	typedef struct tagShotDesc final
	{
		MODE			eMode = { MODE::FIXED };
		ANCHOR_DESC		StartAnchor = {};
		ANCHOR_DESC		EndAnchor = {};
		_float3			vLocalOffset = {};
		_float3			vLookOffset = { 0.f, 0.f, 1.f };
		_float3			vEndLocalOffset = {};
		_float3			vEndLookOffset = { 0.f, 0.f, 1.f };
		_float			fDuration = { 0.f };
		_float			fFovy = { 0.f };
		_float			fLerpSpeed = { 0.f };
		_bool			bLoop = { false };
		_bool			bDeactivateOnFinish = { true };
		_bool			bUsePivotRotation = { true };
	}ATTACH_DESC;

protected:
	CCamera_Cinematic(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CCamera_Cinematic(const CCamera_Cinematic& Prototype);
	virtual ~CCamera_Cinematic() = default;


public:
	virtual HRESULT		Initialize_Prototype() override;
	virtual HRESULT		Initialize(void* pArg) override;
	virtual void		Priority_Update(_float fTimeDelta) override;
	virtual void		Update(_float fTimeDelta) override;
	virtual void		Late_Update(_float fTimeDelta) override;
	virtual HRESULT		Render() override;

public:
	HRESULT				Play(const ATTACH_DESC& Desc);
	HRESULT				Play_Fixed(const _float3& vEye, const _float3& vAt, _float fDuration = 0.f, _float fFovy = 0.f);
	HRESULT				Play_AttachPivot(CGameObject* pObject, const _char* pPivotName,
									const _float3& vLocalOffset, const _float3& vLookOffset,
									_float fDuration = 0.f, _float fFovy = 0.f, const _tchar* pPartTag = nullptr);
	void				Deactivate_Cinematic();
	_bool				Is_CinematicActive() const { return m_bCinematicActive; }
	MODE				Get_Mode() const { return m_eMode; }
	void				Set_ReturnCamera(CCamera* pReturnCamera);

private:
	_bool				Resolve_AnchorWorldMatrix(const ANCHOR_DESC& Anchor, _float4x4* pOutWorld) const;
	_bool				Resolve_ObjectWorldMatrix(CGameObject* pObject, const _tchar* pPartTag, const _char* pPivotName, _float4x4* pOutWorld) const;
	void				Apply_CinematicTransform(_float fTimeDelta);
	void				Apply_EyeAt(_fvector vEye, _fvector vAt, _float fTimeDelta);
	_float				Compute_RailAlpha() const;
	void				Reset_AttachState();
	void				Apply_Fovy(_float fFovy);

private:
	CCamera*			m_pReturnCamera = { nullptr };
	MODE				m_eMode = { MODE::NONE };
	ATTACH_DESC			m_Desc = {};

	_float				m_fDefaultFovy = { 0.f };
	_float				m_fElapsed = { 0.f };
	_wstring			m_strStartPartTag;
	_wstring			m_strEndPartTag;
	string				m_strStartPivotName;
	string				m_strEndPivotName;

	_bool				m_bCinematicActive = { false };
	_bool				m_bHasPreviousFrame = { false };
	_float3				m_vPreviousEye = {};
	_float3				m_vPreviousAt = {};

public:
	static CCamera_Cinematic* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject*	Clone(void* pArg) override;
	virtual void		Free() override;
};

NS_END
