#include "Camera_Follow.h"
#include "SpringArm.h"
#include "GameInstance.h"
#include "Transform_3D.h"
#include "Layer.h"

CCamera_Follow::CCamera_Follow(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CCamera { pDevice, pContext }
{
}

CCamera_Follow::CCamera_Follow(const CCamera_Follow& Prototype)
	: CCamera { Prototype }
{
}

HRESULT CCamera_Follow::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CCamera_Follow::Initialize(void* pArg)
{
	m_strName = TEXT("Camera_");
	m_strTag = TEXT("Follow");

	if (nullptr == pArg)
		return E_FAIL;

	auto pDesc = static_cast<CAMERA_FOLLOW_DESC*>(pArg);

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (FAILED(Ready_Components(*pDesc)))
		return E_FAIL;

	m_strTargetLayerTag = pDesc->strTargetLayerTag.empty()
		? TEXT("Layer_Player")
		: pDesc->strTargetLayerTag;

	return S_OK;
}

void CCamera_Follow::Priority_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	_long lDX = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::X);
	_long lDY = m_pGameInstance->Get_MouseDelta(MOUSEAXIS::Y);

	Rebind_Target();

	m_pSpringArm->Update_Rotation(lDX, lDY);
	m_pSpringArm->Update_Arm(fTimeDelta);

	Apply_SpringArmToTransform();

	__super::Priority_Update(fTimeDelta);
}

void CCamera_Follow::Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	__super::Update(fTimeDelta);
}

void CCamera_Follow::Late_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera())
		return;

	__super::Late_Update(fTimeDelta);
}

HRESULT CCamera_Follow::Render()
{
	return S_OK;
}

_float	CCamera_Follow::Get_Yaw() const
{
	if (nullptr == m_pSpringArm)
		return 0.f;

	return m_pSpringArm->Get_Yaw();
}

HRESULT CCamera_Follow::Ready_Components(const CAMERA_FOLLOW_DESC& Desc)
{
	CSpringArm::SPRING_ARM_DESC ArmDesc{};
	ArmDesc.pTargetWorldMatrix = Desc.pTargetWorldMatrix;
	ArmDesc.vHeightOffset = Desc.vHeightOffset;
	ArmDesc.fIdealDistance = Desc.fIdealDistance;
	ArmDesc.fArmLerpSpeed = Desc.fArmLerpSpeed;
	ArmDesc.fInitialYaw = Desc.fInitialYaw;
	ArmDesc.fInitialPitch = Desc.fInitialPitch;
	ArmDesc.fPitchMin = Desc.fPitchMin;
	ArmDesc.fPitchMax = Desc.fPitchMax;
	ArmDesc.fMouseSensor = Desc.fMouseSensor;

	if (FAILED(__super::Add_Component(
				ETOUI(LEVEL::GAMEPLAY),
				TEXT("Prototype_Component_SpringArm"),
				TEXT("Com_SpringArm"),
				reinterpret_cast<CComponent**>(&m_pSpringArm),
				&ArmDesc)))
		return E_FAIL;

	return S_OK;
}

void CCamera_Follow::Apply_SpringArmToTransform()
{
	_vector vEye		= m_pSpringArm->Get_EyePosition();
	_vector vTarget		= m_pSpringArm->Get_TargetPoint();
	_vector vLook		= m_pSpringArm->Get_LookDirection();

	// World Up ±‚¡ÿ 
	_vector vWorldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector vRight = XMVector3Normalize(XMVector3Cross(vWorldUp, vLook));
	_vector vUp = XMVector3Cross(vLook, vRight);

	CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
	pTransform->Set_State(STATE::RIGHT, vRight);
	pTransform->Set_State(STATE::UP, vUp);
	pTransform->Set_State(STATE::LOOK, vLook);
	pTransform->Set_State(STATE::POSITION, vEye);
}

void CCamera_Follow::Rebind_Target()
{
	if (nullptr == m_pSpringArm)
		return;

	m_pSpringArm->Set_TargetWorldMatrix(Find_TargetWorldMatrix());
}

const _float4x4* CCamera_Follow::Find_TargetWorldMatrix() const
{
	if (m_strTargetLayerTag.empty())
		return nullptr;

	const _int iCurrentLevel = m_pGameInstance->Get_CurrentLevelIndex();
	if (0 > iCurrentLevel)
		return nullptr;

	const auto* pLayers = m_pGameInstance->Get_Layers(static_cast<_uint>(iCurrentLevel));
	if (nullptr == pLayers)
		return nullptr;

	auto iterLayer = pLayers->find(m_strTargetLayerTag);
	if (iterLayer == pLayers->end() || nullptr == iterLayer->second)
		return nullptr;

	const list<CGameObject*>& Objects = iterLayer->second->Get_GameObjects();

	for (CGameObject* pObject : Objects)
	{
		if (nullptr == pObject || nullptr == pObject->Get_Transform())
			continue;

		return pObject->Get_Transform()->Get_WorldMatrixPtr();
	}

	return nullptr;
}

CCamera_Follow* CCamera_Follow::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CCamera_Follow* pInstance = new CCamera_Follow(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CCamera_Follow");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CCamera_Follow::Clone(void* pArg)
{
	CCamera_Follow* pInstance = new CCamera_Follow(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CCamera_Follow");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCamera_Follow::Free()
{
	__super::Free();

	Safe_Release(m_pSpringArm);
}
