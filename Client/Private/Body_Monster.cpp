#include "Body_Monster.h"
#include "GameInstance.h"

#include "Model.h"
#include "Shader.h"
#include "AnimController.h"
#include "MonsterAnimTable.h"
#include "NotifyListener.h"

CBody_Monster::CBody_Monster(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPartObject{ pDevice, pContext }
{
}

CBody_Monster::CBody_Monster(const CBody_Monster& Prototype)
	: CPartObject{ Prototype }
{
}

HRESULT CBody_Monster::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CBody_Monster::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	BODY_MONSTER_DESC Desc = *static_cast<BODY_MONSTER_DESC*>(pArg);
	if (nullptr == Desc.pModelPrototypeTag)
		return E_FAIL;

	m_strName = TEXT("Body_Monster");
	m_strTag = TEXT("Body_Monster");
	m_strModelPrototypeTag = Desc.pModelPrototypeTag;
	m_eAnimSet = Desc.eAnimSet;

	if (FAILED(__super::Initialize(&Desc)))
		return E_FAIL;

	if (FAILED(Ready_Components(Desc)))
		return E_FAIL;

	if (MODEL::ANIM == m_pModelCom->Get_ModelType())
	{
		if (FAILED(Ready_AnimationTable()))
			return E_FAIL;

		if (FAILED(Play_Action(MONSTER_ACTION::IDLE)))
			return E_FAIL;
	}

	return S_OK;
}

void CBody_Monster::Priority_Update(_float fTimeDelta)
{
}

void CBody_Monster::Update(_float fTimeDelta)
{
	if (nullptr == m_pAnimController)
		return;

	_bool bFinished = m_pAnimController->Update(fTimeDelta);

	if (bFinished && nullptr != m_pListener)
	{
		NOTIFY_EVENT Event{};
		Event.eType = NOTIFY_TYPE::ACTION_FINISHED;
		Event.iPayload = Make_MonsterStateKey(m_eCurrentAction, m_eCurrentStep);
		Event.pData = nullptr;

		m_pListener->OnNotify(Event);
	}
}

void CBody_Monster::Late_Update(_float fTimeDelta)
{
	__super::Compute_CombinedWorldMatrix(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()));

	if (nullptr != m_pModelCom)
		m_pGameInstance->Add_RenderGroup(RENDERID::NONBLEND, this);
}

HRESULT CBody_Monster::Render()
{
	if (nullptr == m_pShaderCom || nullptr == m_pModelCom)
		return S_OK;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const _uint iNumMeshes = m_pModelCom->Get_NumMeshes();

	for (_uint i = 0; i < iNumMeshes; ++i)
	{
		if (MODEL::ANIM == m_pModelCom->Get_ModelType())
		{
			if (FAILED(m_pModelCom->Bind_BoneMatrices(m_pShaderCom, "g_BoneMatrices", i)))
				return E_FAIL;
		}

		if (FAILED(m_pModelCom->Bind_Material(m_pShaderCom, "g_DiffuseTexture", i, TEXTURE_TYPE::DIFFUSE)))
			return E_FAIL;

		_float fHasSpec = SUCCEEDED(m_pModelCom->Bind_Material(m_pShaderCom, "g_SpecularTexture", i, TEXTURE_TYPE::SPECULAR)) ? 1.f : 0.f;
		m_pShaderCom->Bind_RawValue("g_fHasSpecularMap", &fHasSpec, sizeof(_float));

		if (FAILED(m_pShaderCom->Begin(0)))
			return E_FAIL;

		if (FAILED(m_pModelCom->Render(i)))
			return E_FAIL;
	}

	return S_OK;
}

const _float4x4* CBody_Monster::Get_BoneMatrixPtr(const _char* pBoneName) const
{
	if (nullptr == m_pModelCom || nullptr == pBoneName)
		return nullptr;

	return m_pModelCom->Get_BoneMatrixPtr(pBoneName);
}

HRESULT CBody_Monster::Play_Action(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep, MONSTER_PHASE ePhase)
{
	const _uint64 iKey = Make_MonsterAnimKey(eAction, ePhase, eStep);

	const MONSTER_ACTION_POLICY* pPolicy = Find_ActionPolicy(eAction, eStep);
	const _float fBlendTime = (nullptr != pPolicy) ? pPolicy->fEnterBlendTime : 0.f;

	if (FAILED(m_pAnimController->Play(iKey, fBlendTime)))
		return E_FAIL;

	m_eCurrentAction = eAction;
	m_eCurrentStep = eStep;
	m_ePhase = ePhase;

	return S_OK;
}

void CBody_Monster::Set_Listener(INotifyListener* pListener)
{
	m_pListener = pListener;

	if (nullptr != m_pAnimController)
		m_pAnimController->Set_Listener(pListener);
}

_float3 CBody_Monster::Get_LastRootMotionDelta() const
{
	if (nullptr == m_pModelCom)
		return _float3{};

	return m_pModelCom->Get_LastRootMotionDelta();
}

HRESULT CBody_Monster::Ready_Components(const BODY_MONSTER_DESC& Desc)
{
	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		Desc.pModelPrototypeTag,
		TEXT("Com_Model"),
		reinterpret_cast<CComponent**>(&m_pModelCom))))
		return E_FAIL;

	const _tchar* pShaderPrototypeTag = (MODEL::ANIM == m_pModelCom->Get_ModelType())
		? TEXT("Prototype_Component_Shader_VtxAnimMesh")
		: TEXT("Prototype_Component_Shader_VtxMesh");

	if (FAILED(__super::Add_Component(
		ETOUI(LEVEL::GAMEPLAY),
		pShaderPrototypeTag,
		TEXT("Com_Shader"),
		reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (MODEL::ANIM == m_pModelCom->Get_ModelType())
	{
		if (FAILED(__super::Add_Component(
			ETOUI(LEVEL::GAMEPLAY),
			TEXT("Prototype_Component_AnimController"),
			TEXT("Com_AnimController"),
			reinterpret_cast<CComponent**>(&m_pAnimController))))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT CBody_Monster::Bind_ShaderResources()
{
	if (FAILED(m_pShaderCom->Bind_Matrix("g_WorldMatrix", &m_CombinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	return S_OK;
}

HRESULT CBody_Monster::Ready_AnimationTable()
{
	m_pAnimTable = Find_MonsterAnimTable(m_eAnimSet);
	if (nullptr == m_pAnimTable)
		return E_FAIL;

	if (nullptr == m_pAnimController || nullptr == m_pModelCom)
		return E_FAIL;

	if (nullptr != m_pAnimTable->pRootBoneName)
		m_pModelCom->Set_RootBoneName(m_pAnimTable->pRootBoneName);

	if (FAILED(m_pAnimController->Bind_Model(m_pModelCom)))
		return E_FAIL;

	if (FAILED(Register_AnimationClips()))
		return E_FAIL;

	if (nullptr != m_pListener)
		m_pAnimController->Set_Listener(m_pListener);

	return S_OK;
}

HRESULT CBody_Monster::Register_AnimationClips()
{
	for (_uint i = 0; i < m_pAnimTable->iNumClips; ++i)
	{
		const MONSTER_ANIM_BIND_DESC& Bind = m_pAnimTable->pClips[i];

		_int iAnimationIndex = m_pModelCom->Get_AnimationIndex(Bind.pAnimationName);
		if (-1 == iAnimationIndex)
			return E_FAIL;

		if (true == Bind.bOverrideLoop)
			m_pModelCom->Set_AnimationLoop(static_cast<_uint>(iAnimationIndex), Bind.bLoop);

		if (true == Bind.bOverrideRootMotion)
			m_pModelCom->Set_AnimationUseRootMotion(static_cast<_uint>(iAnimationIndex), Bind.bUseRootMotion);

		CAnimController::ANIM_CLIP_DESC Desc{};
		Desc.pAnimationName = Bind.pAnimationName;
		Desc.bRestartOnEnter = Bind.bRestartOnEnter;

		if (FAILED(m_pAnimController->Register_Clip(
			Make_MonsterAnimKey(Bind.eAction, Bind.ePhase, Bind.eStep), Desc)))
			return E_FAIL;
	}

	return S_OK;
}

const MONSTER_ACTION_POLICY* CBody_Monster::Find_ActionPolicy(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep) const
{
	if (nullptr == m_pAnimTable)
		return nullptr;

	for (_uint i = 0; i < m_pAnimTable->iNumPolicies; ++i)
	{
		const MONSTER_ACTION_POLICY& Policy = m_pAnimTable->pPolicies[i];

		if (Policy.eAction == eAction && Policy.eStep == eStep)
			return &Policy;
	}

	return nullptr;
}

CBody_Monster* CBody_Monster::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CBody_Monster* pInstance = new CBody_Monster(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CBody_Monster");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CBody_Monster::Clone(void* pArg)
{
	CBody_Monster* pInstance = new CBody_Monster(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CBody_Monster");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CBody_Monster::Free()
{
	__super::Free();

	Safe_Release(m_pAnimController);
	Safe_Release(m_pModelCom);
	Safe_Release(m_pShaderCom);
}
