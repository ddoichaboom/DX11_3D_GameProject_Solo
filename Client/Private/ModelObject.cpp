#include "ModelObject.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Model.h"

CModelObject::CModelObject(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject { pDevice, pContext }
{
}

CModelObject::CModelObject(const CModelObject& Prototype)
	: CGameObject { Prototype }
{
}

HRESULT CModelObject::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CModelObject::Initialize(void* pArg)
{
    MODELOBJECT_DESC* pDesc = static_cast<MODELOBJECT_DESC*>(pArg);
    if (nullptr == pDesc || nullptr == pDesc->pShaderProtoTag || nullptr ==
        pDesc->pModelProtoTag)
        return E_FAIL;

    m_strShaderProtoTag = pDesc->pShaderProtoTag;
    m_strModelProtoTag = pDesc->pModelProtoTag;

    m_strName = TEXT("ModelObject");

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    //m_pTransformCom->Set_Scale(_float3(0.1f, 0.1f, 0.1f));
    
    return S_OK;
}

void CModelObject::Priority_Update(_float fTimeDelta)
{
}

void CModelObject::Update(_float fTimeDelta)
{
    if (MODEL::ANIM == m_pModelCom->Get_ModelType())
        m_pModelCom->Play_Animation(fTimeDelta);
}

void CModelObject::Late_Update(_float fTimeDelta)
{
    m_pGameInstance->Add_RenderGroup(RENDERID::NONBLEND, this);
}

HRESULT CModelObject::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    _uint iNumMeshes = m_pModelCom->Get_NumMeshes();

    for (_uint i = 0; i < iNumMeshes; ++i)
    {
        if (MODEL::ANIM == m_pModelCom->Get_ModelType())
            m_pModelCom->Bind_BoneMatrices(m_pShaderCom, "g_BoneMatrices", i);

        m_pModelCom->Bind_Material(m_pShaderCom, "g_DiffuseTexture",
            i, TEXTURE_TYPE::DIFFUSE);

        if (FAILED(m_pShaderCom->Begin(0)))
            return E_FAIL;

        if (FAILED(m_pModelCom->Render(i)))
            return E_FAIL;

    }
    
    return S_OK;
}

HRESULT CModelObject::Ready_Components()
{
    // Com_Shader
    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        m_strShaderProtoTag.c_str(),
        TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
        return E_FAIL;

    // Com_Model
    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        m_strModelProtoTag.c_str(),
        TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
        return E_FAIL;

    return S_OK;
}

HRESULT CModelObject::Bind_ShaderResources()
{
    if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
        return E_FAIL;

    return S_OK;
}

CModelObject* CModelObject::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CModelObject* pInstance = new CModelObject(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CModelObject");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CGameObject* CModelObject::Clone(void* pArg)
{
    CModelObject* pInstance = new CModelObject(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CModelObject");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CModelObject::Free()
{
    __super::Free();

    Safe_Release(m_pShaderCom);
    Safe_Release(m_pModelCom);
}
