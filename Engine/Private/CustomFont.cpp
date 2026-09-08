#include "CustomFont.h"

CCustomFont::CCustomFont(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice{ pDevice }
	, m_pContext{ pContext }
{
	Safe_AddRef(m_pDevice);
	Safe_AddRef(m_pContext);
}

HRESULT CCustomFont::Initialize(const _tchar* pFontFilePath)
{
	m_pBatch = new SpriteBatch(m_pContext);
	m_pFont = new SpriteFont(m_pDevice, pFontFilePath);
	m_pFont->SetDefaultCharacter(L'?');

	return S_OK;
}

HRESULT CCustomFont::Draw(const _tchar* pText, const _float2& vPosition, _fvector vColor, _float fRotation, const _float2& vOrigin, const _float2& vScale)
{
	// SpriteBatch  전에 GS 초기화 필수 ( 3D 렌더 후 잔류  GS가 SpriteBatch를 오작동 시킨다.)
	m_pContext->GSSetShader(nullptr, nullptr, 0);

	m_pBatch->Begin();
	m_pFont->DrawString(m_pBatch, pText, vPosition, vColor, fRotation, vOrigin, vScale);
	m_pBatch->End();

	return S_OK;
}

HRESULT CCustomFont::Measure(const _tchar* pText, _float2* pOutSize)
{
	if (nullptr == m_pFont || nullptr == pText || nullptr == pOutSize)
		return E_FAIL;

	XMVECTOR vSize = m_pFont->MeasureString(pText);
	pOutSize->x = XMVectorGetX(vSize);
	pOutSize->y = XMVectorGetY(vSize);
	return S_OK;
}

CCustomFont* CCustomFont::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const _tchar* pFontFilePath)
{
	CCustomFont* pInstance = new CCustomFont(pDevice, pContext);
	if (FAILED(pInstance->Initialize(pFontFilePath)))
	{
		MSG_BOX("Failed to Created : CCustomFont");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCustomFont::Free()
{
	__super::Free();

	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);

	Safe_Delete(m_pBatch);
	Safe_Delete(m_pFont);
}
