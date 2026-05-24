#include "Camera_Free.h"
#include "GameInstance.h"
#include "Transform_3D.h"

CCamera_Free::CCamera_Free(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CCamera{ pDevice, pContext }
{
}

CCamera_Free::CCamera_Free(const CCamera_Free& Prototype)
	: CCamera { Prototype }
{
}

HRESULT CCamera_Free::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CCamera_Free::Initialize(void* pArg)
{
	m_strName = TEXT("Camera_");
	m_strTag = TEXT("Free");

	if (nullptr == pArg)
		return E_FAIL;

	auto pDesc = static_cast<CAMERA_FREE_DESC*>(pArg);

	m_fMouseSensor = pDesc->fMouseSensor;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	return S_OK;
}

void CCamera_Free::Priority_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	const _bool bFastMove = (m_pGameInstance->Get_KeyState(VK_SHIFT) & 0x80) != 0;

	const _float fMoveTimeDelta = fTimeDelta * (bFastMove ? 2.f : 1.f);

	if (m_pGameInstance->Get_MouseBtnState(MOUSEBTN::RBUTTON) & 0x80)
	{
		// WASD 이동
		if (m_pGameInstance->Get_KeyState('W') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Straight(fMoveTimeDelta);

		if (m_pGameInstance->Get_KeyState('S') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Backward(fMoveTimeDelta);

		if (m_pGameInstance->Get_KeyState('A') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Left(fMoveTimeDelta);

		if (m_pGameInstance->Get_KeyState('D') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Right(fMoveTimeDelta);

		if (m_pGameInstance->Get_KeyState('Q') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Down(fMoveTimeDelta);

		if (m_pGameInstance->Get_KeyState('E') & 0x80)
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Up(fMoveTimeDelta);

		// 마우스 회전
		_long MouseMove = {};

		// 수평 이동 -> Y축 기준 회전 (Yaw)
		if (MouseMove = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::X))
		{
			static_cast<CTransform_3D*>(m_pTransformCom)->Turn(
				XMVectorSet(0.f, 1.f, 0.f, 0.f),
				MouseMove * m_fMouseSensor * fTimeDelta);
		}

		// 수직 이동 -> Right 축 기준 회전 (Pitch)
		if (MouseMove = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::Y))
		{
			static_cast<CTransform_3D*>(m_pTransformCom)->Turn(
				m_pTransformCom->Get_State(STATE::RIGHT),
				MouseMove * m_fMouseSensor * fTimeDelta);
		}

		_long lWheel = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::WHEEL);
		if (lWheel)
		{
			static_cast<CTransform_3D*>(m_pTransformCom)->Go_Straight(
				fTimeDelta * lWheel * m_fMouseSensor);
		}
	}

	// 부모 (CCamera)의 Priority_Update 호출
	__super::Priority_Update(fTimeDelta);
}

void CCamera_Free::Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	__super::Update(fTimeDelta);
}

void CCamera_Free::Late_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	__super::Late_Update(fTimeDelta);
}

HRESULT CCamera_Free::Render()
{
	return S_OK;
}

CCamera_Free* CCamera_Free::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CCamera_Free* pInstance = new CCamera_Free(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CCamera_Free");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CCamera_Free::Clone(void* pArg)
{
	CCamera_Free* pInstance = new CCamera_Free(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CCamera_Free");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCamera_Free::Free()
{
	__super::Free();
}
