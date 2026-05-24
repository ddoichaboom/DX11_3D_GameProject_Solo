#pragma once

#include "Base.h"

// 1. 화면에 그려져야할 객체들을 그리는 순서대로 모아놓는다. (그룹으로 구분)
// 2. 보관된 순서대로 객체들의 드로우콜을 해준다.

NS_BEGIN(Engine)

class CRenderer final : public CBase
{
private:
	CRenderer(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CRenderer() = default;

public:
	HRESULT						Initialize();
	void						Add_RenderGroup(RENDERID eGroupID, class CGameObject* pGameObject);
	HRESULT						Draw();

	HRESULT						Resize(_uint iWidth, _uint iHeight);

#ifdef _DEBUG
public:
	HRESULT                     Resize_DebugRenderTargets(_uint iCanvasWidth, _uint iCanvasHeight);
#endif 


private:
	ID3D11Device*				m_pDevice = { nullptr };
	ID3D11DeviceContext*		m_pContext = { nullptr };

#ifdef _DEBUG
private:
	HRESULT                     Render_Debug();

	HRESULT                     Ready_DebugRenderTargets(_uint iCanvasWidth, _uint iCanvasHeight);
#endif

private:
	list<class CGameObject*>    m_RenderObjects[ETOUI(RENDERID::END)];
	class CGameInstance*		m_pGameInstance = { nullptr };

	class CShader*				m_pShader = { nullptr };
	class CVIBuffer_Rect*		m_pVIBuffer = { nullptr };

private:
	_float4x4					m_WorldMatrix = {};
	_float4x4					m_ViewMatrix = {};
	_float4x4					m_ProjMatrix = {};

#ifdef _DEBUG
private:
	_bool						m_bRenderTargetDebug = { false };
	_uint						m_iDeferredDebugView = { 0 };
#endif

private:
	HRESULT						Render_Priority();
	HRESULT						Render_NonBlend();
	HRESULT						Render_Blend();
	HRESULT						Render_UI();

	HRESULT						Render_Lights();
	HRESULT						Render_Combined();
	HRESULT						Render_NonLight();

	HRESULT						Force_ViewportAlpha();


public:
	static CRenderer* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void Free() override;
};

NS_END