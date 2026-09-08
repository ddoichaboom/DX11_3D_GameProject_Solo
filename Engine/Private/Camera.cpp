#include "Camera.h"
#include "Transform_3D.h"
#include "GameInstance.h"

CCamera::CCamera(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{

}

CCamera::CCamera(const CCamera& Prototype)
	: CGameObject { Prototype }
{

}

HRESULT CCamera::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CCamera::Initialize(void* pArg)
{
	auto	pDesc		= static_cast<CAMERA_DESC*>(pArg);

	// (1) 뷰포트에서 Aspect Ratio 자동 계산
	D3D11_VIEWPORT		ViewPortDesc{};
	_uint				iNumViewports = { 1 };
	m_pContext->RSGetViewports(&iNumViewports, &ViewPortDesc);

	m_fNear				= pDesc->fNear;
	m_fAspect			= static_cast<_float>(ViewPortDesc.Width) / ViewPortDesc.Height;
	m_fFar				= pDesc->fFar;
	m_fFovy				= pDesc->fFovy;

	// (2) Transform 생성
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	// (3) 카메라 위치/방향 설정
	m_pTransformCom->Set_State(STATE::POSITION, XMVectorSetW(XMLoadFloat3(&pDesc->vEye), 1.f));
	
	// LookAt은 CTransform_3D 전용이므로 다운 캐스팅
	static_cast<CTransform_3D*>(m_pTransformCom)->LookAt(XMVectorSetW(XMLoadFloat3(&pDesc->vAt), 1.f));

	// (4) 투영 행렬 생성
	XMStoreFloat4x4(&m_ProjMatrix,
		XMMatrixPerspectiveFovLH(m_fFovy, m_fAspect, m_fNear, m_fFar));

	// (5) 초기 상태를 PipeLine에 세팅
	Update_PipeLine();

	return S_OK;
}

void CCamera::Priority_Update(_float fTimeDelta)
{

}

void CCamera::Update(_float fTimeDelta)
{
	// 매 프레임 PipeLine에 View/Proj 업데이트
	Update_PipeLine();
}

void CCamera::Late_Update(_float fTimeDelta)
{

}

HRESULT CCamera::Render()
{
	return S_OK;
}

void CCamera::Update_PipeLine()
{
	if (false == m_bActiveCamera)
		return;

	// View = Inverse(Camera WorldMatrix)
	m_pGameInstance->Set_Transform(D3DTS::VIEW,
		XMMatrixInverse(nullptr, XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr())));

	m_pGameInstance->Set_Transform(D3DTS::PROJ,
		XMLoadFloat4x4(&m_ProjMatrix));
}

void CCamera::Resize_Projection(_uint iWidth, _uint iHeight)
{
	if (0 == iWidth || 0 == iHeight)
		return;

	m_fAspect = static_cast<_float>(iWidth) / static_cast<_float>(iHeight);

	XMStoreFloat4x4(
		&m_ProjMatrix,
		XMMatrixPerspectiveFovLH(m_fFovy, m_fAspect, m_fNear, m_fFar));

	Update_PipeLine();
}

void CCamera::Free()
{
	__super::Free();
}
