#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class  CShadow final : public CBase
{
private:
	CShadow();
	virtual ~CShadow() = default;

public:
	const _float4x4*		Get_Transform(D3DTS eState) const 
	{
		return &m_TransformMatrices[ETOUI(eState)];
	}

public:
	HRESULT					Add_ShadowLight(const SHADOW_LIGHT_DESC& ShadowDesc);

private:
	class CGameInstance*	m_pGameInstance = { nullptr };

private:
	_float4x4				m_TransformMatrices[ETOUI(D3DTS::END)] = {};

public:
	static CShadow*			Create();
	virtual void			Free() override;
};

NS_END