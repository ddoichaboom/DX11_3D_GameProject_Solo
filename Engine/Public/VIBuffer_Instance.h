#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CVIBuffer_Instance abstract : public CVIBuffer
{
protected:
	CVIBuffer_Instance(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CVIBuffer_Instance(const CVIBuffer_Instance& Prototype);
	virtual ~CVIBuffer_Instance() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;

	virtual HRESULT Bind_Resources() override;
	virtual HRESULT Render() override;

	_uint Get_NumInstances() const { return m_iNumInstances; }
	_uint Get_InstanceCapacity() const { return m_iInstanceCapacity; }

protected:
	HRESULT Create_InstanceBuffer(const void* pInitialData, _uint iInstanceCapacity, _uint iInstanceStride);
	HRESULT Update_InstanceBuffer(const void* pData, _uint iNumInstances, _uint iInstanceStride);

protected:
	ID3D11Buffer*			m_pVBInstance = { nullptr };
	_uint					m_iNumInstances = {};
	_uint					m_iInstanceCapacity = {};
	_uint					m_iInstanceStride = {};
	_uint					m_iIndexCountPerInstance = {};
	D3D11_BUFFER_DESC		m_InstanceBufferDesc = {};

public:
	virtual CComponent* Clone(void* pArg) PURE;
	virtual void Free() override;
};

NS_END
