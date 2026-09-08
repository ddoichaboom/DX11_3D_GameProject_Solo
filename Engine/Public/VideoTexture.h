#pragma once
#include "Engine_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL CVideoTexture final : public CBase
{
private:
	CVideoTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CVideoTexture() = default;

public:
	HRESULT						Initialize(const _tchar* pVideoPath, _bool bLoop);

	HRESULT						Update(_float fTimeDelta);
	HRESULT						Upload_ReadyFrame();
	void						Reset();

	void						Set_Loop(_bool bLoop) { m_bLoop = bLoop; }
	void						Set_Speed(_float fSpeed) { m_fPlaybackSpeed = fSpeed; }
	_bool						Is_Finished() const { return m_bFinished; }
	_uint						Get_Width() const { return m_iVideoWidth; }
	_uint						Get_Height() const { return m_iVideoHeight; }

	ID3D11ShaderResourceView*	Get_SRV() const { return m_pSRV; }

private:
	ID3D11Device*				m_pDevice = { nullptr };
	ID3D11DeviceContext*		m_pContext = { nullptr };
	IMFSourceReader*			m_pSourceReader = { nullptr };
	IMFSample*					m_pPendingSample = { nullptr };

	ID3D11Texture2D*			m_pTexture = { nullptr };
	ID3D11ShaderResourceView*	m_pSRV = { nullptr };

	_uint						m_iVideoWidth = { 0 };
	_uint						m_iVideoHeight = { 0 };
	_uint						m_iSourceStride = { 0 };

	LONGLONG					m_llPlaybackTime = { 0 };    // 100ns ¥‹¿ß
	LONGLONG					m_llPendingPTS = { 0 };
	_bool						m_bHasPendingSample = { false };

	_bool						m_bLoop = { true };
	_bool						m_bFinished = { false };
	_float						m_fPlaybackSpeed = { 1.f };

private:
	HRESULT						Configure_SourceReader();
	HRESULT						Create_DynamicTexture();
	HRESULT						Read_NextSample();
	HRESULT						Upload_PendingSample();
	void						Release_PendingSample();


public:
	static CVideoTexture*		Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const _tchar* pVideoPath, _bool bLoop = true);
	virtual void				Free() override;
};

NS_END