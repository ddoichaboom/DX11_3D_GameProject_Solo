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

private:
	ID3D11Device*				m_pDevice = { nullptr };
	ID3D11DeviceContext*		m_pContext = { nullptr };

#ifdef _DEBUG
private:
	HRESULT                     Render_Debug();
#endif

private:
	list<class CGameObject*>    m_RenderObjects[ETOUI(RENDERID::END)];
	class CGameInstance*		m_pGameInstance = { nullptr };

	class CShader*				m_pShader = { nullptr };
	class CVIBuffer_Rect*		m_pVIBuffer = { nullptr };

	_float4x4					m_WorldMatrix = {};
	_float4x4					m_ViewMatrix = {};
	_float4x4					m_ProjMatrix = {};

private:
	HRESULT						Render_Priority();
	HRESULT						Render_NonBlend();
	HRESULT						Render_Blend();
	HRESULT						Render_UI();

	HRESULT						Render_Lights();
	HRESULT						Render_Combined();
	HRESULT						Render_NonLight();


public:
	static CRenderer* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void Free() override;
};

NS_END