#include "Renderer.h"
#include "GameObject.h"
#include "UIObject.h"
#include "GameInstance.h"
#include "Shader.h"
#include "VIBuffer_Rect.h"

namespace
{
	constexpr _uint SHADOW_MAP_SIZE = 4096;
}

CRenderer::CRenderer(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice { pDevice }
	, m_pContext{ pContext }
	, m_pGameInstance { CGameInstance::GetInstance() }
{
	Safe_AddRef(m_pGameInstance);
	Safe_AddRef(m_pDevice);
	Safe_AddRef(m_pContext);
}

HRESULT CRenderer::Initialize()
{
	const _uint iWinSizeX = m_pGameInstance->Get_WinSizeX();
	const _uint iWinSizeY = m_pGameInstance->Get_WinSizeY();

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Viewport"), iWinSizeX, iWinSizeY,	DXGI_FORMAT_R8G8B8A8_UNORM,	_float4(0.2f, 0.2f, 0.2f, 1.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Diffuse"), iWinSizeX, iWinSizeY, DXGI_FORMAT_R8G8B8A8_UNORM, _float4(0.f, 0.f, 0.f, 0.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Normal"), iWinSizeX, iWinSizeY, DXGI_FORMAT_R16G16B16A16_UNORM, _float4(0.f, 0.f, 0.f, 1.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Shade"), iWinSizeX, iWinSizeY, DXGI_FORMAT_R16G16B16A16_UNORM, _float4(0.f, 0.f, 0.f, 1.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Specular"), iWinSizeX, iWinSizeY, DXGI_FORMAT_R16G16B16A16_UNORM, _float4(0.f, 0.f, 0.f, 0.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_Depth"), iWinSizeX, iWinSizeY, DXGI_FORMAT_R32G32B32A32_FLOAT, _float4(0.f, 0.f, 0.f, 0.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_RenderTarget(TEXT("Target_LightDepth"),	SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, DXGI_FORMAT_R32G32B32A32_FLOAT, _float4(1.f, 1.f, 1.f, 1.f))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_GameObjects"), TEXT("Target_Diffuse"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_GameObjects"), TEXT("Target_Normal"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_GameObjects"), TEXT("Target_Depth"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_LightAcc"), TEXT("Target_Shade"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_LightAcc"), TEXT("Target_Specular"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Add_MRT(TEXT("MRT_ShadowObjects"), TEXT("Target_LightDepth"))))
		return E_FAIL;

	if (FAILED(Ready_ShadowDepthStencil_Buffer()))
		return E_FAIL;

	m_pShader = CShader::Create(
		m_pDevice,
		m_pContext,
		TEXT("../../Resources/ShaderFiles/Shader_Deferred.hlsl"),
		VTXTEX::Elements,
		VTXTEX::iNumElements);

	if (nullptr == m_pShader)
		return E_FAIL;

	m_pVIBuffer = CVIBuffer_Rect::Create(m_pDevice, m_pContext);
	if (nullptr == m_pVIBuffer)
		return E_FAIL;

	if (FAILED(Resize(iWinSizeX, iWinSizeY)))
		return E_FAIL;

#ifdef _DEBUG
	if (FAILED(Ready_DebugRenderTargets(iWinSizeX, iWinSizeY)))
		return E_FAIL;
#endif

	return S_OK;
}

void CRenderer::Add_RenderGroup(RENDERID eGroupID, CGameObject* pGameObject)
{
	if (nullptr == pGameObject)
		return;

	m_RenderObjects[ETOUI(eGroupID)].push_back(pGameObject);

	// 해당 객체가 원래 있어야 하는 곳이 아닌 사용되는 곳에 담기는 것이므로 레퍼런스 카운트 증가
	Safe_AddRef(pGameObject);
}

HRESULT CRenderer::Draw()
{
	if (FAILED(Render_Priority()))
		return E_FAIL;

	if (FAILED(Render_Shadow()))
		return E_FAIL;

	if (FAILED(Render_NonBlend()))
		return E_FAIL;

	if (FAILED(Render_Lights()))
		return E_FAIL;

	if (FAILED(Render_Combined()))
		return E_FAIL;

	if (FAILED(Render_NonLight()))
		return E_FAIL;

	if (FAILED(Render_Blend()))
		return E_FAIL;

	if (FAILED(Render_UI()))
		return E_FAIL;

#ifdef _DEBUG
	if (m_pGameInstance->Get_KeyDown(VK_F9))
		m_bRenderTargetDebug = !m_bRenderTargetDebug;

	if (m_pGameInstance->Get_KeyDown(VK_F10))
		m_iDeferredDebugView = (m_iDeferredDebugView + 1) % 7;

	if (m_bRenderTargetDebug)
	{
		if (FAILED(Render_Debug()))
			return E_FAIL;
	}
#endif

	if (FAILED(Force_ViewportAlpha()))
		return E_FAIL;

	return S_OK;

}

HRESULT CRenderer::Resize(_uint iWidth, _uint iHeight)
{
	if (0 == iWidth || 0 == iHeight)
		return E_FAIL;

	XMStoreFloat4x4(
		&m_WorldMatrix,
		XMMatrixScaling(
			static_cast<_float>(iWidth),
			static_cast<_float>(iHeight),
			1.f));

	XMStoreFloat4x4(&m_ViewMatrix, XMMatrixIdentity());

	XMStoreFloat4x4(
		&m_ProjMatrix,
		XMMatrixOrthographicLH(
			static_cast<_float>(iWidth),
			static_cast<_float>(iHeight),
			0.f,
			1.f));

	return S_OK;
}

#ifdef _DEBUG

HRESULT CRenderer::Resize_DebugRenderTargets(_uint iCanvasWidth, _uint iCanvasHeight)
{
	return Ready_DebugRenderTargets(iCanvasWidth, iCanvasHeight);
}

#endif

HRESULT CRenderer::Render_Priority()
{
	for (auto& pRenderObject : m_RenderObjects[ETOUI(RENDERID::PRIORITY)])
	{
		if (nullptr != pRenderObject)
			pRenderObject->Render();

		Safe_Release(pRenderObject);
	}

	m_RenderObjects[ETOUI(RENDERID::PRIORITY)].clear();

	return S_OK;
}

HRESULT CRenderer::Render_Shadow()
{
	if (FAILED(m_pGameInstance->Begin_MRT(TEXT("MRT_ShadowObjects"), m_pShadowDSV)))
		return E_FAIL;

	m_pContext->ClearDepthStencilView(m_pShadowDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	for (auto& pRenderObject : m_RenderObjects[ETOUI(RENDERID::SHADOW)])
	{
		if (nullptr != pRenderObject)
		{
			if (FAILED(pRenderObject->Render_Shadow()))
				return E_FAIL;
		}

		Safe_Release(pRenderObject);
	}

	m_RenderObjects[ETOUI(RENDERID::SHADOW)].clear();

	if (FAILED(m_pGameInstance->End_MRT()))
		return E_FAIL;

	return S_OK;
}

HRESULT CRenderer::Render_NonBlend()
{
	if (FAILED(m_pGameInstance->Begin_MRT(TEXT("MRT_GameObjects"))))
		return E_FAIL;

	for (auto& pRenderObject : m_RenderObjects[ETOUI(RENDERID::NONBLEND)])
	{
		if (nullptr != pRenderObject)
			pRenderObject->Render();

		Safe_Release(pRenderObject);
	}

	m_RenderObjects[ETOUI(RENDERID::NONBLEND)].clear();

	if (FAILED(m_pGameInstance->End_MRT()))
		return E_FAIL;

	return S_OK;
}

HRESULT CRenderer::Render_Blend()
{
	for (auto& pRenderObject : m_RenderObjects[ETOUI(RENDERID::BLEND)])
	{
		if (nullptr != pRenderObject)
			pRenderObject->Render();

		Safe_Release(pRenderObject);
	}

	m_RenderObjects[ETOUI(RENDERID::BLEND)].clear();

	return S_OK;
}

HRESULT CRenderer::Render_UI()
{
	auto& UIObjects = m_RenderObjects[ETOUI(RENDERID::UI)];

	UIObjects.sort(
		[](CGameObject* pA, CGameObject* pB)
		{
			CUIObject* pUIA = dynamic_cast<CUIObject*>(pA);
			CUIObject* pUIB = dynamic_cast<CUIObject*>(pB);

			const _uint zA = pUIA ? pUIA->Get_ZOrder() : 0;
			const _uint zB = pUIB ? pUIB->Get_ZOrder() : 0;

			return zA < zB;
		});

	for (auto& pRenderObject : UIObjects)
	{
		if (nullptr != pRenderObject)
			pRenderObject->Render();

		Safe_Release(pRenderObject);
	}

	UIObjects.clear();

	return S_OK;
}

HRESULT CRenderer::Render_Lights()
{
#ifdef _DEBUG
	static _uint iPrevLightCount = UINT_MAX;
	const _uint iLightCount = m_pGameInstance->Get_NumLights();

	if (iPrevLightCount != iLightCount)
	{
		_tchar szDebug[128] = {};
		swprintf_s(szDebug, TEXT("[Renderer] Light Count : %u\n"), iLightCount);
		OutputDebugString(szDebug);

		iPrevLightCount = iLightCount;
	}
#endif

	if (FAILED(m_pGameInstance->Begin_MRT(TEXT("MRT_LightAcc"))))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Normal"), m_pShader, "g_NormalTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Depth"), m_pShader, "g_DepthTexture")))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_WorldMatrix", &m_WorldMatrix)))
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrix", &m_ViewMatrix)))
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrix", &m_ProjMatrix)))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrixInverse", m_pGameInstance->Get_Transform_Inverse(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrixInverse", m_pGameInstance->Get_Transform_Inverse(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_RawValue("g_vCamPosition", m_pGameInstance->Get_CamPosition(), sizeof(_float4))))
		return E_FAIL;

	if (FAILED(m_pVIBuffer->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Render_Light(m_pShader, m_pVIBuffer)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->End_MRT()))
		return E_FAIL;

	return S_OK;
}

HRESULT CRenderer::Render_Combined()
{
	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Diffuse"), m_pShader, "g_DiffuseTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Normal"), m_pShader, "g_NormalTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Depth"), m_pShader, "g_DepthTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Shade"), m_pShader, "g_ShadeTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_Specular"), m_pShader, "g_SpecularTexture")))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Bind_RT_ShaderResource(TEXT("Target_LightDepth"), m_pShader, "g_LightDepthTexture")))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_WorldMatrix", &m_WorldMatrix)))
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrix", &m_ViewMatrix)))
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrix", &m_ProjMatrix)))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrixInverse", m_pGameInstance->Get_Transform_Inverse(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrixInverse", m_pGameInstance->Get_Transform_Inverse(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ShadowLightViewMatrix", m_pGameInstance->Get_Shadow_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ShadowLightProjMatrix", m_pGameInstance->Get_Shadow_Transform(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pVIBuffer->Bind_Resources()))
		return E_FAIL;

	_uint iPassIndex = ETOUI(DEFERRED::COMBINED);

#ifdef _DEBUG
	switch (m_iDeferredDebugView)
	{
	case 1:
		iPassIndex = ETOUI(DEFERRED::COMBINED_DIFFUSE);
		break;
	case 2:
		iPassIndex = ETOUI(DEFERRED::COMBINED_NORMAL);
		break;
	case 3:
		iPassIndex = ETOUI(DEFERRED::COMBINED_DEPTH);
		break;
	case 4:
		iPassIndex = ETOUI(DEFERRED::COMBINED_SHADE);
		break;
	case 5:
		iPassIndex = ETOUI(DEFERRED::COMBINED_SPECULAR);
		break;
	case 6:
		iPassIndex = ETOUI(DEFERRED::COMBINED_LIGHT_DEPTH);
		break;
	default:
		iPassIndex = ETOUI(DEFERRED::COMBINED);
		break;
	}
#endif

	if (FAILED(m_pShader->Begin(iPassIndex)))
		return E_FAIL;

	if (FAILED(m_pVIBuffer->Render()))
		return E_FAIL;

	ID3D11ShaderResourceView* pNullSRVs[8] = {};
	m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

	return S_OK;
}

HRESULT CRenderer::Render_NonLight()
{
	for (auto& pRenderObject : m_RenderObjects[ETOUI(RENDERID::NONLIGHT)])
	{
		if (nullptr != pRenderObject)
			pRenderObject->Render();

		Safe_Release(pRenderObject);
	}

	m_RenderObjects[ETOUI(RENDERID::NONLIGHT)].clear();

	return S_OK;
}

HRESULT CRenderer::Force_ViewportAlpha()
{
	if (FAILED(m_pShader->Bind_Matrix("g_WorldMatrix", &m_WorldMatrix))) 
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrix", &m_ViewMatrix)))   
		return E_FAIL;
	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrix", &m_ProjMatrix)))   
		return E_FAIL;
	if (FAILED(m_pVIBuffer->Bind_Resources()))                          
		return E_FAIL;
	if (FAILED(m_pShader->Begin(ETOUI(DEFERRED::FORCE_ALPHA))))          
		return E_FAIL;
	if (FAILED(m_pVIBuffer->Render()))                                   
		return E_FAIL;

	return S_OK;
}

HRESULT CRenderer::Ready_ShadowDepthStencil_Buffer()
{
	Safe_Release(m_pShadowDSV);
	Safe_Release(m_pShadowDSTexture);

	D3D11_TEXTURE2D_DESC TextureDesc{};
	TextureDesc.Width = SHADOW_MAP_SIZE;
	TextureDesc.Height = SHADOW_MAP_SIZE;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	if (FAILED(m_pDevice->CreateTexture2D(&TextureDesc, nullptr, &m_pShadowDSTexture)))
		return E_FAIL;

	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc{};
	DSVDesc.Format = TextureDesc.Format;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	if (FAILED(m_pDevice->CreateDepthStencilView(m_pShadowDSTexture, &DSVDesc, &m_pShadowDSV)))
		return E_FAIL;

	return S_OK;
}

#ifdef _DEBUG

HRESULT CRenderer::Render_Debug()
{
	if (FAILED(m_pShader->Bind_Matrix("g_ViewMatrix", &m_ViewMatrix)))
		return E_FAIL;

	if (FAILED(m_pShader->Bind_Matrix("g_ProjMatrix", &m_ProjMatrix)))
		return E_FAIL;

	if (FAILED(m_pVIBuffer->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Render_RT_Debug(TEXT("MRT_GameObjects"), m_pShader, m_pVIBuffer)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Render_RT_Debug(TEXT("MRT_LightAcc"), m_pShader, m_pVIBuffer)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Render_RT_Debug(TEXT("MRT_ShadowObjects"), m_pShader, m_pVIBuffer)))
		return E_FAIL;

	ID3D11ShaderResourceView* pNullSRVs[8] = {};
	m_pContext->PSSetShaderResources(0, 8, pNullSRVs);

	return S_OK;
}

HRESULT CRenderer::Ready_DebugRenderTargets(_uint iCanvasWidth, _uint iCanvasHeight)
{
	const _float fCanvasWidth = static_cast<_float>(iCanvasWidth);
	const _float fCanvasHeight = static_cast<_float>(iCanvasHeight);

	if (FAILED(m_pGameInstance->Ready_RT_Debug(TEXT("Target_Diffuse"), 160.f, 90.f, 320.f, 180.f, fCanvasWidth, fCanvasHeight)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Ready_RT_Debug(TEXT("Target_Normal"), 160.f, 270.f, 320.f, 180.f, fCanvasWidth, fCanvasHeight)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Ready_RT_Debug(TEXT("Target_Shade"), 160.f, 450.f, 320.f, 180.f, fCanvasWidth, fCanvasHeight)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Ready_RT_Debug(TEXT("Target_Specular"), 480.f, 90.f, 320.f, 180.f, fCanvasWidth, fCanvasHeight)))
		return E_FAIL;

	if (FAILED(m_pGameInstance->Ready_RT_Debug(TEXT("Target_LightDepth"), 480.f, 270.f, 320.f, 180.f, fCanvasWidth, fCanvasHeight)))
		return E_FAIL;

	return S_OK;
}

#endif

CRenderer* CRenderer::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CRenderer* pInstance = new CRenderer(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : CRenderer");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CRenderer::Free()
{
	__super::Free();

	for (auto& RenderObjects : m_RenderObjects)
	{
		for (auto& pRenderObject : RenderObjects)
			Safe_Release(pRenderObject);

		RenderObjects.clear();
	}

	Safe_Release(m_pShadowDSV);
	Safe_Release(m_pShadowDSTexture);
	Safe_Release(m_pShader);
	Safe_Release(m_pVIBuffer);
	Safe_Release(m_pGameInstance);
	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);
}
