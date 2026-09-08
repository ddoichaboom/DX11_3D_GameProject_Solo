#pragma once

#include "VIBuffer_Instance.h"

NS_BEGIN(Engine)

class ENGINE_DLL CVIBuffer_Rect_Instance final : public CVIBuffer_Instance
{
public:
	typedef struct tagRectInstanceDesc
	{
		_uint						iMaxInstanceCount = {};
		const VTXRECT_INSTANCE*		pInitialInstances = { nullptr };
		_uint						iInitialInstanceCount = {};
	}RECT_INSTANCE_DESC;

private:
	CVIBuffer_Rect_Instance(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CVIBuffer_Rect_Instance(const CVIBuffer_Rect_Instance& Prototype);
	virtual ~CVIBuffer_Rect_Instance() = default;

public:
	HRESULT Initialize_Prototype(_uint iMaxInstanceCount);
	virtual HRESULT Initialize(void* pArg) override;

	HRESULT Set_Instances(const vector<VTXRECT_INSTANCE>& Instances);
	HRESULT Set_Instances(const VTXRECT_INSTANCE* pInstances, _uint iNumInstances);
	HRESULT Update_Billboard(const vector<_float4>& Positions, const _float2& vSize, const _float4& vTexInfo, const _float4& vColor, _fmatrix ViewMatrix);
	HRESULT Update_Billboard(const vector<_float4>& Positions, const _float2& vSize, const vector<_float4>& TexInfos, const vector<_float4>& Colors, _fmatrix ViewMatrix);

private:
	_uint						m_iMaxInstanceCount = {};
	vector<VTXRECT_INSTANCE>	m_Instances;

public:
	static CVIBuffer_Rect_Instance* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, _uint iMaxInstanceCount);
	virtual CComponent* Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
