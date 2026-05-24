#include "Body_Player.h"
#include "GameInstance.h"
#include "Player.h"

#include "Model.h"
#include "Shader.h"
#include "AnimController.h"
#include "NotifyListener.h"

CBody_Player::CBody_Player(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPartObject { pDevice, pContext }
{
}

CBody_Player::CBody_Player(const CBody_Player& Prototype)
	: CPartObject{ Prototype }
{
}

const _float4x4* CBody_Player::Get_BoneMatrixPtr(const _char* pBoneName) const
{
	return m_pModelCom->Get_BoneMatrixPtr(pBoneName);
}

_float3   CBody_Player::Get_LastRootMotionDelta() const
{
    return m_pModelCom->Get_LastRootMotionDelta();
}

HRESULT CBody_Player::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CBody_Player::Initialize(void* pArg)
{
    m_strName = TEXT("Body_Player");
    m_strTag = TEXT("Body_Player");

    auto pDesc = static_cast<BODY_PLAYER_DESC*>(pArg);

    m_pParentState = pDesc->pParentState;

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    if (MODEL::ANIM == m_pModelCom->Get_ModelType())
    {
        if (FAILED(Ready_AnimationTable()))
            return E_FAIL;

        if (FAILED(Play_Action(CHARACTER_ACTION::IDLE)))
            return E_FAIL;
    }

    return S_OK;
}

void CBody_Player::Priority_Update(_float fTimeDelta)
{
}

void CBody_Player::Update(_float fTimeDelta)
{
    if (m_pGameInstance->Is_GameLogic_Frozen())
    {
        if (nullptr != m_pModelCom)
            m_pModelCom->Play_Animation(fTimeDelta);

        return;
    }

    _bool bFinished = m_pAnimController->Update(fTimeDelta);

    if (bFinished && nullptr != m_pListener)
    {
        NOTIFY_EVENT Event{};
        Event.eType = NOTIFY_TYPE::ACTION_FINISHED;
        Event.iPayload = Make_PlayerStateKey(m_eCurrentAction, m_eCurrentStep);
        Event.pData = nullptr;

        m_pListener->OnNotify(Event);
    }
}

void CBody_Player::Late_Update(_float fTimeDelta)
{
    __super::Compute_CombinedWorldMatrix(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()));

    m_pGameInstance->Add_RenderGroup(RENDERID::NONBLEND, this);
}

HRESULT CBody_Player::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    size_t iNumMeshes = m_pModelCom->Get_NumMeshes();

    for (size_t i = 0; i < iNumMeshes; i++)
    {
        if (FAILED(m_pModelCom->Bind_Material(m_pShaderCom, "g_DiffuseTexture", i, TEXTURE_TYPE::DIFFUSE, 0)))
            return E_FAIL;

        if (FAILED(m_pModelCom->Bind_BoneMatrices(m_pShaderCom, "g_BoneMatrices", i)))
            return E_FAIL;

        _float fHasSpec = SUCCEEDED(m_pModelCom->Bind_Material(m_pShaderCom, "g_SpecularTexture", i, TEXTURE_TYPE::SPECULAR, 0)) ? 1.f : 0.f;
        m_pShaderCom->Bind_RawValue("g_fHasSpecularMap", &fHasSpec, sizeof(_float));

        if (FAILED(m_pShaderCom->Begin(0)))
            return E_FAIL;

        if (FAILED(m_pModelCom->Render(i)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT CBody_Player::Ready_Components()
{
    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_Shader_VtxAnimMesh"),
        TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_Model_SungJinWoo_OverDrive"),
        TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_AnimController"),
        TEXT("Com_AnimController"),
        reinterpret_cast<CComponent**>(&m_pAnimController))))
        return E_FAIL;


    return S_OK;
}

HRESULT CBody_Player::Bind_ShaderResources()
{
    if (FAILED(m_pShaderCom->Bind_Matrix("g_WorldMatrix", &m_CombinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
        return E_FAIL;

    return S_OK;
}

HRESULT CBody_Player::Ready_AnimationTable()
{
    m_pAnimTable = Find_CharacterAnimTable(m_eCharacterType);

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

HRESULT CBody_Player::Register_AnimationClips()
{
    if (nullptr == m_pAnimController || nullptr == m_pAnimTable)
        return E_FAIL;

    for (size_t i = 0; i < m_pAnimTable->iNumBinds; i++)
    {
        const CHARACTER_ANIM_BIND_DESC& Bind = m_pAnimTable->pBinds[i];

        CAnimController::ANIM_CLIP_DESC Desc{};
        Desc.pAnimationName = Bind.pAnimationName;
        Desc.bRestartOnEnter = Bind.bRestartOnEnter;

        if (FAILED(m_pAnimController->Register_Clip(
            Make_CharacterAnimKey(Bind.eState, Bind.eAction, Bind.eStep, Bind.eWeapon, Bind.eEquippedId), Desc)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT CBody_Player::Play_Action(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep)
{
    if (nullptr == m_pAnimController || nullptr == m_pAnimTable)
        return E_FAIL;

    const _uint64 iKey = Resolve_ActionKey(eAction, eStep);

    const CHARACTER_ACTION_POLICY* pPolicy = Find_ActionPolicy(eAction, eStep);
    const _float fBlendTime = (nullptr != pPolicy) ? pPolicy->fEnterBlendTime : 0.f;

    if (FAILED(m_pAnimController->Play(iKey, fBlendTime)))
        return E_FAIL;

    m_eCurrentAction = eAction;
    m_eCurrentStep = eStep;

    return S_OK;
}

void    CBody_Player::Set_Listener(INotifyListener* pListener)
{
    m_pListener = pListener;

    if (nullptr != m_pAnimController)
        m_pAnimController->Set_Listener(pListener);
}

CHARACTER_ACTION CBody_Player::Pick_RunEndAction() const
{
    // Foot(발) 본의 모델 로컬 Z값 비교 - 더 작은 z (뒤쪽에 위치) 쪽 발로 멈춤
    if (nullptr == m_pModelCom)
        return CHARACTER_ACTION::RUN_END;      // Clip Run_End에 바인딩할지 고민

    const _float4x4* pLFoot = m_pModelCom->Get_BoneMatrixPtr("FX_Point_L_Foot");
    const _float4x4* pRFoot = m_pModelCom->Get_BoneMatrixPtr("FX_Point_R_Foot");

    if (nullptr == pLFoot || nullptr == pRFoot)
        return CHARACTER_ACTION::RUN_END;

    return (pLFoot->_43 < pRFoot->_43)
        ? CHARACTER_ACTION::RUN_END_LEFT
        : CHARACTER_ACTION::RUN_END_RIGHT;
}

const CHARACTER_ANIM_BIND_DESC* CBody_Player::Find_Bind(CHARACTER_STATE eState, CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep, WEAPON_TYPE eWeapon, EQUIPPED_WEAPON_ID eEquippedId) const
{
    if (nullptr == m_pAnimTable)
        return nullptr;

    for (size_t i = 0; i < m_pAnimTable->iNumBinds; ++i)
    {
        const CHARACTER_ANIM_BIND_DESC& Bind = m_pAnimTable->pBinds[i];

        if (Bind.eState == eState && Bind.eAction == eAction && Bind.eStep == eStep && Bind.eWeapon == eWeapon && Bind.eEquippedId == eEquippedId)
            return &Bind;
    }

    return nullptr;
}

_uint64 CBody_Player::Resolve_ActionKey(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep)
{
    if (nullptr == m_pAnimTable)
        return 0;

    auto try_match = [&](CHARACTER_STATE eState, CHARACTER_ACTION_STEP eStep, WEAPON_TYPE eWeapon, EQUIPPED_WEAPON_ID eEquippedId) -> const CHARACTER_ANIM_BIND_DESC*
        {
            for (size_t i = 0; i < m_pAnimTable->iNumBinds; i++)
            {
                const CHARACTER_ANIM_BIND_DESC& bindDesc = m_pAnimTable->pBinds[i];
                if (bindDesc.eState == eState && bindDesc.eAction == eAction && bindDesc.eStep == eStep && bindDesc.eWeapon == eWeapon && bindDesc.eEquippedId == eEquippedId)
                    return &bindDesc;
            }
            return nullptr;
        };

    auto try_match_anystate = [&](CHARACTER_ACTION_STEP eStep, WEAPON_TYPE eWeapon, EQUIPPED_WEAPON_ID eEquippedId) -> const CHARACTER_ANIM_BIND_DESC*
        {
            for (size_t i = 0; i < m_pAnimTable->iNumBinds; i++)
            {
                const CHARACTER_ANIM_BIND_DESC& bindDesc = m_pAnimTable->pBinds[i];
                if (bindDesc.eAction == eAction && bindDesc.eStep == eStep && bindDesc.eWeapon == eWeapon && bindDesc.eEquippedId == eEquippedId)
                    return &bindDesc;
            }
            return nullptr;
        };

    // STATE + WEAPON 매칭 → EQUIPPED_ID 우선, 없으면 NONE 폴백
    const CHARACTER_ANIM_BIND_DESC* pBind = try_match(m_eCurrentState, eStep, m_eWeaponState, m_eEquippedWeaponId);
    if (nullptr == pBind)
        pBind = try_match(m_eCurrentState, eStep, m_eWeaponState, EQUIPPED_WEAPON_ID::NONE);

    // STATE + DEFAULT 폴백
    if (nullptr == pBind)
        pBind = try_match(m_eCurrentState, eStep, WEAPON_TYPE::DEFAULT, EQUIPPED_WEAPON_ID::NONE);

    // ANYSTATE + WEAPON 폴백 (EQUIPPED_ID 우선, 없으면 NONE)
    if (nullptr == pBind)
        pBind = try_match_anystate(eStep, m_eWeaponState, m_eEquippedWeaponId);
    if (nullptr == pBind)
        pBind = try_match_anystate(eStep, m_eWeaponState, EQUIPPED_WEAPON_ID::NONE);

    // ANYSTATE + DEFAULT 최종 폴백
    if (nullptr == pBind)
        pBind = try_match_anystate(eStep, WEAPON_TYPE::DEFAULT, EQUIPPED_WEAPON_ID::NONE);

    if (nullptr == pBind)
        return 0;

    m_eCurrentState = pBind->eState;

    return Make_CharacterAnimKey(pBind->eState, eAction, pBind->eStep, pBind->eWeapon, pBind->eEquippedId);
}

const CHARACTER_ACTION_POLICY* CBody_Player::Find_ActionPolicy(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep) const
{
    if (nullptr == m_pAnimTable)
        return nullptr;

    for (size_t i = 0; i < m_pAnimTable->iNumPolicies; i++)
    {
        const CHARACTER_ACTION_POLICY& Policy = m_pAnimTable->pPolicies[i];

        if (Policy.eAction == eAction && Policy.eStep == eStep)
            return &Policy;
    }

    return nullptr;
}

CBody_Player* CBody_Player::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CBody_Player* pInstance = new CBody_Player(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CBody_Player");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CGameObject* CBody_Player::Clone(void* pArg)
{
    CBody_Player* pInstance = new CBody_Player(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CBody_Player");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CBody_Player::Free()
{
    __super::Free();

    Safe_Release(m_pAnimController);
    Safe_Release(m_pModelCom);
    Safe_Release(m_pShaderCom);    
}