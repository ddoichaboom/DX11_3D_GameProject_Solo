#include "Shadow.h"
#include "GameInstance.h"

CShadow::CShadow()
	: m_pGameInstance{ CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);

	for (_uint i = 0; i < ETOUI(D3DTS::END); i++)
		XMStoreFloat4x4(&m_TransformMatrices[i], XMMatrixIdentity());		
}

HRESULT CShadow::Add_ShadowLight(const SHADOW_LIGHT_DESC& ShadowDesc)
{
    XMStoreFloat4x4(
        &m_TransformMatrices[ETOUI(D3DTS::VIEW)],
        XMMatrixLookAtLH(
            XMLoadFloat4(&ShadowDesc.vEye),
            XMLoadFloat4(&ShadowDesc.vAt),
            XMVectorSet(0.f, 1.f, 0.f, 0.f)));

    const _float fAspect = ShadowDesc.fAspect > 0.f 
        ? ShadowDesc.fAspect 
        : static_cast<_float>(m_pGameInstance->Get_WinSizeX()) /
            static_cast<_float>(m_pGameInstance->Get_WinSizeY());

    XMStoreFloat4x4(
        &m_TransformMatrices[ETOUI(D3DTS::PROJ)],
        XMMatrixPerspectiveFovLH(
            ShadowDesc.fFovy,
            fAspect,
            ShadowDesc.fNear,
            ShadowDesc.fFar));

    return S_OK;
}

CShadow* CShadow::Create()
{
	return new CShadow();
}

void CShadow::Free()
{
	__super::Free();

	Safe_Release(m_pGameInstance);
}
