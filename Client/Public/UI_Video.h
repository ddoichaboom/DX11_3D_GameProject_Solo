#pragma once

#include "Client_Defines.h"
#include "UIObject.h"

NS_BEGIN(Engine)
class CShader;
class CVIBuffer;
class CVideoTexture;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CUI_Video : public CUIObject
{
public:
	typedef struct tagUI_VideoDesc : public CUIObject::UIOBJECT_DESC
	{
		const _tchar*	pVideoPath = { nullptr };
		_bool			bLoop = { true };
		_float			fPlaybackSpeed = { 1.f };
	}UI_VIDEO_DESC;

protected:
	CUI_Video(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CUI_Video(const CUI_Video& Prototype);
	virtual ~CUI_Video() = default;

public:
	void						Play();
	void						Pause();
	void						Stop();
	void						Reset();
	_bool						Is_Playing() const { return m_bPlaying; }
	_bool						Is_Finished() const;

	virtual HRESULT				Initialize_Prototype() override;
	virtual HRESULT				Initialize(void* pArg) override;
	virtual void				Update(_float fTimeDelta) override;
	virtual void				Late_Update(_float fTimeDelta) override;
	virtual HRESULT				Render() override;

private:
	CShader*					m_pShaderCom = { nullptr };
	CVIBuffer*					m_pVIBufferCom = { nullptr };
	CVideoTexture*				m_pVideoTexture = { nullptr };
	_bool						m_bPlaying = { false };

private:
	HRESULT						Ready_Components(const _tchar* pVideoPath, _bool bLoop, _float fSpeed);
	HRESULT						Bind_ShaderResources();

public:
	static CUI_Video*			Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CGameObject*		Clone(void* pArg) override;
	virtual void				Free() override;
};

NS_END
