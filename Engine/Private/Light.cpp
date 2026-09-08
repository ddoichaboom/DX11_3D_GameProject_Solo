#include "Light.h"
#include "Shader.h"
#include "VIBuffer_Rect.h"

CLight::CLight(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice{ pDevice }
	, m_pContext{ pContext }
{
	Safe_AddRef(m_pDevice);
	Safe_AddRef(m_pContext);
}

HRESULT CLight::Initialize(const LIGHT_DESC& LightDesc)
{
	m_LightDesc = LightDesc;

	return S_OK;
}

HRESULT CLight::Render(CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
    _uint iPassIndex = {};

    if (LIGHT::DIRECTIONAL == m_LightDesc.eType)
    {
        if (FAILED(pShader->Bind_RawValue("g_vLightDir", &m_LightDesc.vDirection, sizeof m_LightDesc.vDirection)))
            return E_FAIL;

        iPassIndex = ETOUI(DEFERRED::DIRECTIONAL);
    }
    else
    {
        if (FAILED(pShader->Bind_RawValue("g_vLightPos", &m_LightDesc.vPosition, sizeof m_LightDesc.vPosition)))
            return E_FAIL;

        if (FAILED(pShader->Bind_RawValue("g_fLightRange", &m_LightDesc.fRange, sizeof m_LightDesc.fRange)))
            return E_FAIL;

        iPassIndex = ETOUI(DEFERRED::POINT);
    }

    if (FAILED(pShader->Bind_RawValue("g_vLightDiffuse", &m_LightDesc.vDiffuse, sizeof m_LightDesc.vDiffuse)))
        return E_FAIL;

    if (FAILED(pShader->Bind_RawValue("g_vLightAmbient", &m_LightDesc.vAmbient, sizeof m_LightDesc.vAmbient)))
        return E_FAIL;

    if (FAILED(pShader->Bind_RawValue("g_vLightSpecular", &m_LightDesc.vSpecular, sizeof m_LightDesc.vSpecular)))
        return E_FAIL;

    if (FAILED(pShader->Begin(iPassIndex)))
        return E_FAIL;

    if (FAILED(pVIBuffer->Render()))
        return E_FAIL;

    return S_OK;
}

CLight* CLight::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const LIGHT_DESC& LightDesc)
{
	CLight* pInstance = new CLight(pDevice, pContext);

	if (FAILED(pInstance->Initialize(LightDesc)))
	{
		MSG_BOX("Failed to Created : CLight");
		Safe_Release(pInstance);
	};

	return pInstance;
}

void CLight::Free()
{
	__super::Free();

	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);
}

