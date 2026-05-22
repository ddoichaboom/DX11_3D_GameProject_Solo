#include "Weapon.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Model.h"
#include "Collider.h"

CWeapon::CWeapon(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CPartObject{ pDevice, pContext }
{
}

CWeapon::CWeapon(const CWeapon& Prototype)
    : CPartObject{ Prototype }
{
}

HRESULT CWeapon::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CWeapon::Initialize(void* pArg)
{
    m_strName = TEXT("Weapon");
    m_strTag = TEXT("");

    auto pDesc = static_cast<WEAPON_DESC*>(pArg);

    m_pSocketBoneMatrix     = pDesc->pSocketBoneMatrix;
    m_pModelPrototypeTag    = pDesc->pModelPrototypeTag;
    m_bVisible              = pDesc->bInitiallyVisible;
    m_eAttackGroup          = pDesc->eAttackGroup;

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    if (FAILED(Ready_BladeCollider()))
        return E_FAIL;

    return S_OK;
}

void CWeapon::Priority_Update(_float fTimeDelta)
{
}

void CWeapon::Update(_float fTimeDelta)
{
}

void CWeapon::Late_Update(_float fTimeDelta)
{
    if (nullptr == m_pSocketBoneMatrix || nullptr == m_pParentMatrix)
        return;

    _matrix SocketMatrix = XMLoadFloat4x4(m_pSocketBoneMatrix);

    for (size_t i = 0; i < 3; i++)
        SocketMatrix.r[i] = XMVector3Normalize(SocketMatrix.r[i]);

    Compute_CombinedWorldMatrix(XMLoadFloat4x4(m_pTransformCom->Get_WorldMatrixPtr()) * SocketMatrix);

    if (false == m_bVisible)
        return;

    m_pGameInstance->Add_RenderGroup(RENDERID::NONBLEND, this);

    Update_BladeHitbox();
}

HRESULT CWeapon::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    size_t iNumMeshes = m_pModelCom->Get_NumMeshes();

    for (size_t i = 0; i < iNumMeshes; i++)
    {
        if (FAILED(m_pModelCom->Bind_Material(m_pShaderCom, "g_DiffuseTexture", i, TEXTURE_TYPE::DIFFUSE, 0)))
            return E_FAIL;

        if (FAILED(m_pShaderCom->Begin(0)))
            return E_FAIL;

        if (FAILED(m_pModelCom->Render(i)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT CWeapon::Set_Model(const _tchar* pModelPrototypeTag)
{
    if (nullptr == pModelPrototypeTag)
        return E_FAIL;

    // 동일 모델이면 스킵
    if (nullptr != pModelPrototypeTag && 0 == lstrcmpW(m_pModelPrototypeTag, pModelPrototypeTag))
        return S_OK;

    // 새 모델 Clone
    CModel* pNewModel = dynamic_cast<CModel*>(
        m_pGameInstance->Clone_Prototype(
            PROTOTYPE::COMPONENT,
            ETOUI(LEVEL::GAMEPLAY),
            pModelPrototypeTag,
            nullptr));

    if (nullptr == pNewModel)
        return E_FAIL;

    // 기존 모델 components map에서 제거 + 해제
    auto iter = m_Components.find(TEXT("Com_Model"));
    if (iter != m_Components.end())
    {
        Safe_Release(iter->second);
        m_Components.erase(iter);
    }

    // 멤버 포인터의 ref도 해제
    Safe_Release(m_pModelCom);

    // 새 모델 등록
    m_Components.emplace(TEXT("Com_Model"), pNewModel);
    m_pModelCom = pNewModel;
    Safe_AddRef(m_pModelCom);

    m_pModelPrototypeTag = pModelPrototypeTag;

    return S_OK;
}

HRESULT CWeapon::Ready_Components()
{
    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_Shader_VtxMesh"),
        TEXT("Com_Shader"), reinterpret_cast<CComponent**>(&m_pShaderCom))))
        return E_FAIL;

    if (nullptr == m_pModelPrototypeTag)
        return E_FAIL;

    if (FAILED(__super::Add_Component(ETOUI(LEVEL::GAMEPLAY),
        m_pModelPrototypeTag,
        TEXT("Com_Model"), reinterpret_cast<CComponent**>(&m_pModelCom))))
        return E_FAIL;

    return S_OK;
}

HRESULT CWeapon::Bind_ShaderResources()
{
    if (FAILED(m_pShaderCom->Bind_Matrix("g_WorldMatrix", &m_CombinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
        return E_FAIL;

    return S_OK;
}

HRESULT CWeapon::Ready_BladeCollider()
{
    m_pBladeCollider = CCollider::Create(m_pDevice, m_pContext);
    if (nullptr == m_pBladeCollider)
        return E_FAIL;

    CCollider::COLLIDER_DESC Desc{};
    Desc.eBoundingType = COLLIDER::OBB;
    Desc.vCenter = _float3(0.f, 0.f, 0.f);
    Desc.vSize = _float3(1.f, 1.f, 1.f);
    Desc.vRadians = _float3(0.f, 0.f, 0.f);
    Desc.eGroup = m_eAttackGroup;
    Desc.pOwner = this;

    if (FAILED(m_pBladeCollider->Initialize(&Desc)))
        return E_FAIL;

    return S_OK;
}

void CWeapon::Update_BladeHitbox()
{
    if (nullptr == m_pBladeCollider || nullptr == m_pModelCom)
        return;

    _float3 vCenter, vHalfExtent;
    if (false == m_pModelCom->Get_LocalAABB(vCenter, vHalfExtent))
        return;

    _matrix BoxLocal = XMMatrixScaling(vHalfExtent.x * 2.f, vHalfExtent.y * 2.f, vHalfExtent.z * 2.f)
        * XMMatrixTranslation(vCenter.x, vCenter.y, vCenter.z);

    _matrix BoxWorld = BoxLocal * XMLoadFloat4x4(&m_CombinedWorldMatrix);

    m_pBladeCollider->Update(BoxWorld);

    if (true == m_bAttackHitboxActive)
        m_pBladeCollider->Register();
}

CWeapon* CWeapon::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CWeapon* pInstance = new CWeapon(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CWeapon");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CGameObject* CWeapon::Clone(void* pArg)
{
    CWeapon* pInstance = new CWeapon(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CWeapon");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CWeapon::Free()
{
    __super::Free();

    Safe_Release(m_pBladeCollider);
    Safe_Release(m_pModelCom);
    Safe_Release(m_pShaderCom);
}