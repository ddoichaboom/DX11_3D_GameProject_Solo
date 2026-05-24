#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class CLight_Manager final : public CBase
{
private:
	CLight_Manager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CLight_Manager() = default;

public:
	const LIGHT_DESC*			Get_LightDesc(_uint iIndex);
	_uint						Get_NumLights() const { return static_cast<_uint>(m_Lights.size()); }

public:
	HRESULT						Add_Light(const LIGHT_DESC& LightDesc);
	HRESULT                     Render(class CShader* pShader, class CVIBuffer_Rect* pVIBuffer);

private:
	ID3D11Device*				m_pDevice = { nullptr };
	ID3D11DeviceContext*		m_pContext = { nullptr };

	list<class CLight*>			m_Lights;

public:
	static CLight_Manager*		Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void				Free() override;

};

NS_END