#include "Camera_Cinematic.h"
#include "GameInstance.h"
#include "Transform_3D.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "Model.h"

CCamera_Cinematic::CCamera_Cinematic(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CCamera{ pDevice, pContext }
{
}

CCamera_Cinematic::CCamera_Cinematic(const CCamera_Cinematic& Prototype)
	: CCamera{ Prototype }
{
}

HRESULT CCamera_Cinematic::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CCamera_Cinematic::Initialize(void* pArg)
{
	m_strName = TEXT("Camera_");
	m_strTag = TEXT("Cinematic");

	if (nullptr == pArg)
		return E_FAIL;

	auto pDesc = static_cast<CAMERA_CINEMATIC_DESC*>(pArg);

	m_bActiveCamera = pDesc->bActive;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_fDefaultFovy = m_fFovy;
	Set_ActiveCamera(pDesc->bActive);

	return S_OK;
}

void CCamera_Cinematic::Priority_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera() || false == m_bCinematicActive)
		return;

	Apply_CinematicTransform(fTimeDelta);

	__super::Priority_Update(fTimeDelta);
}

void CCamera_Cinematic::Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera() || false == m_bCinematicActive)
		return;

	if (m_Desc.fDuration > 0.f)
	{
		m_fElapsed += fTimeDelta;

		if (m_fElapsed >= m_Desc.fDuration)
		{
			if (true == m_Desc.bLoop)
				m_fElapsed = fmodf(m_fElapsed, m_Desc.fDuration);
			else if (true == m_Desc.bDeactivateOnFinish)
			{
				Deactivate_Cinematic();
				return;
			}
			else
				m_fElapsed = m_Desc.fDuration;
		}
	}

	__super::Update(fTimeDelta);
}

void CCamera_Cinematic::Late_Update(_float fTimeDelta)
{
	if (false == Is_ActiveCamera() || false == m_bCinematicActive)
		return;

	__super::Late_Update(fTimeDelta);
}

HRESULT CCamera_Cinematic::Render()
{
	return S_OK;
}

HRESULT CCamera_Cinematic::Play(const ATTACH_DESC& Desc)
{
	if (MODE::NONE == Desc.eMode)
		return E_FAIL;

	Reset_AttachState();

	m_Desc = Desc;
	m_eMode = Desc.eMode;
	m_fElapsed = 0.f;
	m_bCinematicActive = true;
	m_bHasPreviousFrame = false;

	if (nullptr != Desc.StartAnchor.pPartTag)
		m_strStartPartTag = Desc.StartAnchor.pPartTag;
	if (nullptr != Desc.EndAnchor.pPartTag)
		m_strEndPartTag = Desc.EndAnchor.pPartTag;
	if (nullptr != Desc.StartAnchor.pPivotName)
		m_strStartPivotName = Desc.StartAnchor.pPivotName;
	if (nullptr != Desc.EndAnchor.pPivotName)
		m_strEndPivotName = Desc.EndAnchor.pPivotName;

	Safe_AddRef(m_Desc.StartAnchor.pObject);
	Safe_AddRef(m_Desc.EndAnchor.pObject);

	if (m_fDefaultFovy <= 0.f)
		m_fDefaultFovy = m_fFovy;

	Apply_Fovy(Desc.fFovy);
	Set_ActiveCamera(true);
	Apply_CinematicTransform(0.f);
	Update_PipeLine();

	return S_OK;
}

HRESULT CCamera_Cinematic::Play_Fixed(const _float3& vEye, const _float3& vAt, _float fDuration, _float fFovy)
{
	ATTACH_DESC Desc{};
	Desc.eMode = MODE::FIXED;
	Desc.StartAnchor.vWorldPosition = vEye;
	Desc.StartAnchor.bUseWorldPosition = true;
	Desc.EndAnchor.vWorldPosition = vAt;
	Desc.EndAnchor.bUseWorldPosition = true;
	Desc.fDuration = fDuration;
	Desc.fFovy = fFovy;

	return Play(Desc);
}

HRESULT CCamera_Cinematic::Play_AttachPivot(CGameObject* pObject, const _char* pPivotName,
	const _float3& vLocalOffset, const _float3& vLookOffset, _float fDuration, _float fFovy, const _tchar* pPartTag)
{
	ATTACH_DESC Desc{};
	Desc.eMode = MODE::ATTACH_PIVOT;
	Desc.StartAnchor.pObject = pObject;
	Desc.StartAnchor.pPartTag = pPartTag;
	Desc.StartAnchor.pPivotName = pPivotName;
	Desc.vLocalOffset = vLocalOffset;
	Desc.vLookOffset = vLookOffset;
	Desc.fDuration = fDuration;
	Desc.fFovy = fFovy;

	return Play(Desc);
}

void CCamera_Cinematic::Deactivate_Cinematic()
{
	Set_ActiveCamera(false);
	Apply_Fovy(m_fDefaultFovy);
	Reset_AttachState();
}

_bool CCamera_Cinematic::Resolve_AnchorWorldMatrix(const ANCHOR_DESC& Anchor, _float4x4* pOutWorld) const
{
	if (nullptr == pOutWorld)
		return false;

	if (true == Anchor.bUseWorldPosition)
	{
		XMStoreFloat4x4(pOutWorld, XMMatrixTranslation(Anchor.vWorldPosition.x, Anchor.vWorldPosition.y, Anchor.vWorldPosition.z));
		return true;
	}

	return Resolve_ObjectWorldMatrix(Anchor.pObject, Anchor.pPartTag, Anchor.pPivotName, pOutWorld);
}

_bool CCamera_Cinematic::Resolve_ObjectWorldMatrix(CGameObject* pObject, const _tchar* pPartTag, const _char* pPivotName, _float4x4* pOutWorld) const
{
	if (nullptr == pObject || nullptr == pOutWorld)
		return false;

	CGameObject* pResolvedObject = pObject;

	if (nullptr != pPartTag)
	{
		CContainerObject* pContainer = dynamic_cast<CContainerObject*>(pObject);
		if (nullptr == pContainer)
			return false;

		const auto& PartObjects = pContainer->Get_PartObjects();
		auto iterPart = PartObjects.find(pPartTag);
		if (iterPart == PartObjects.end() || nullptr == iterPart->second)
			return false;

		pResolvedObject = iterPart->second;
	}

	const _float4x4* pObjectWorld = nullptr;

	if (CPartObject* pPart = dynamic_cast<CPartObject*>(pResolvedObject))
		pObjectWorld = &pPart->Get_CombinedWorldMatrix();
	else if (nullptr != pResolvedObject->Get_Transform())
		pObjectWorld = pResolvedObject->Get_Transform()->Get_WorldMatrixPtr();

	if (nullptr == pObjectWorld)
		return false;

	_matrix WorldMatrix = XMLoadFloat4x4(pObjectWorld);

	if (nullptr != pPivotName)
	{
		auto iterModel = pResolvedObject->Get_Components().find(TEXT("Com_Model"));
		if (iterModel == pResolvedObject->Get_Components().end() || nullptr == iterModel->second)
			return false;

		CModel* pModel = dynamic_cast<CModel*>(iterModel->second);
		if (nullptr == pModel)
			return false;

		const _float4x4* pPivotMatrix = pModel->Get_BoneMatrixPtr(pPivotName);
		if (nullptr == pPivotMatrix)
			return false;

		WorldMatrix = XMLoadFloat4x4(pPivotMatrix) * WorldMatrix;
	}

	XMStoreFloat4x4(pOutWorld, WorldMatrix);

	return true;
}

void CCamera_Cinematic::Apply_CinematicTransform(_float fTimeDelta)
{
	_float4x4 StartWorld{};
	_float4x4 EndWorld{};

	ANCHOR_DESC StartAnchor = m_Desc.StartAnchor;
	ANCHOR_DESC EndAnchor = m_Desc.EndAnchor;
	StartAnchor.pPartTag = m_strStartPartTag.empty() ? nullptr : m_strStartPartTag.c_str();
	EndAnchor.pPartTag = m_strEndPartTag.empty() ? nullptr : m_strEndPartTag.c_str();
	StartAnchor.pPivotName = m_strStartPivotName.empty() ? nullptr : m_strStartPivotName.c_str();
	EndAnchor.pPivotName = m_strEndPivotName.empty() ? nullptr : m_strEndPivotName.c_str();

	if (MODE::FIXED == m_eMode)
	{
		if (false == Resolve_AnchorWorldMatrix(StartAnchor, &StartWorld) ||
			false == Resolve_AnchorWorldMatrix(EndAnchor, &EndWorld))
			return;

		Apply_EyeAt(XMLoadFloat4x4(&StartWorld).r[3], XMLoadFloat4x4(&EndWorld).r[3], fTimeDelta);
		return;
	}

	if (false == Resolve_AnchorWorldMatrix(StartAnchor, &StartWorld))
		return;

	_matrix StartMatrix = XMLoadFloat4x4(&StartWorld);
	_vector vStartEye = XMVector3TransformCoord(XMLoadFloat3(&m_Desc.vLocalOffset), StartMatrix);
	_vector vStartAt = XMVector3TransformCoord(XMLoadFloat3(&m_Desc.vLookOffset), StartMatrix);

	if (MODE::ATTACH_PIVOT == m_eMode)
	{
		Apply_EyeAt(vStartEye, vStartAt, fTimeDelta);
		return;
	}

	if (false == Resolve_AnchorWorldMatrix(EndAnchor, &EndWorld))
		return;

	_matrix EndMatrix = XMLoadFloat4x4(&EndWorld);
	_vector vEndEye = XMVector3TransformCoord(XMLoadFloat3(&m_Desc.vEndLocalOffset), EndMatrix);
	_vector vEndAt = XMVector3TransformCoord(XMLoadFloat3(&m_Desc.vEndLookOffset), EndMatrix);
	const _float fAlpha = Compute_RailAlpha();

	Apply_EyeAt(XMVectorLerp(vStartEye, vEndEye, fAlpha), XMVectorLerp(vStartAt, vEndAt, fAlpha), fTimeDelta);
}

void CCamera_Cinematic::Apply_EyeAt(_fvector vEye, _fvector vAt, _float fTimeDelta)
{
	_vector vApplyEye = vEye;
	_vector vApplyAt = vAt;

	if (m_Desc.fLerpSpeed > 0.f && true == m_bHasPreviousFrame && fTimeDelta > 0.f)
	{
		const _float fAlpha = min(1.f, m_Desc.fLerpSpeed * fTimeDelta);
		vApplyEye = XMVectorLerp(XMLoadFloat3(&m_vPreviousEye), vEye, fAlpha);
		vApplyAt = XMVectorLerp(XMLoadFloat3(&m_vPreviousAt), vAt, fAlpha);
	}

	_vector vLook = XMVectorSubtract(vApplyAt, vApplyEye);
	if (XMVectorGetX(XMVector3LengthSq(vLook)) <= 0.0001f)
		return;

	vLook = XMVector3Normalize(vLook);

	_vector vWorldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector vRight = XMVector3Cross(vWorldUp, vLook);

	if (XMVectorGetX(XMVector3LengthSq(vRight)) <= 0.0001f)
		vRight = XMVectorSet(1.f, 0.f, 0.f, 0.f);
	else
		vRight = XMVector3Normalize(vRight);

	_vector vUp = XMVector3Normalize(XMVector3Cross(vLook, vRight));

	CTransform_3D* pTransform = static_cast<CTransform_3D*>(m_pTransformCom);
	pTransform->Set_State(STATE::RIGHT, vRight);
	pTransform->Set_State(STATE::UP, vUp);
	pTransform->Set_State(STATE::LOOK, vLook);
	pTransform->Set_State(STATE::POSITION, vApplyEye);

	XMStoreFloat3(&m_vPreviousEye, vApplyEye);
	XMStoreFloat3(&m_vPreviousAt, vApplyAt);
	m_bHasPreviousFrame = true;
}

_float CCamera_Cinematic::Compute_RailAlpha() const
{
	if (m_Desc.fDuration <= 0.f)
		return 1.f;

	_float fAlpha = min(1.f, max(0.f, m_fElapsed / m_Desc.fDuration));

	if (MODE::PINGPONG == m_eMode)
	{
		fAlpha *= 2.f;
		if (fAlpha > 1.f)
			fAlpha = 2.f - fAlpha;
	}

	return fAlpha;
}

void CCamera_Cinematic::Reset_AttachState()
{
	Safe_Release(m_Desc.StartAnchor.pObject);
	Safe_Release(m_Desc.EndAnchor.pObject);

	m_Desc = {};
	m_eMode = MODE::NONE;
	m_fElapsed = 0.f;
	m_strStartPartTag.clear();
	m_strEndPartTag.clear();
	m_strStartPivotName.clear();
	m_strEndPivotName.clear();
	m_bCinematicActive = false;
	m_bHasPreviousFrame = false;
	m_vPreviousEye = {};
	m_vPreviousAt = {};
}

void CCamera_Cinematic::Apply_Fovy(_float fFovy)
{
	if (fFovy <= 0.f)
		return;

	m_fFovy = fFovy;
	XMStoreFloat4x4(&m_ProjMatrix, XMMatrixPerspectiveFovLH(m_fFovy, m_fAspect, m_fNear, m_fFar));
}

CCamera_Cinematic* CCamera_Cinematic::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CCamera_Cinematic* pInstance = new CCamera_Cinematic(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CCamera_Cinematic");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CCamera_Cinematic::Clone(void* pArg)
{
	CCamera_Cinematic* pInstance = new CCamera_Cinematic(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CCamera_Cinematic");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CCamera_Cinematic::Free()
{
	Reset_AttachState();

	__super::Free();
}
