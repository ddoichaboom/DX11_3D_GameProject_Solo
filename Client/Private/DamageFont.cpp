#include "DamageFont.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect_Instance.h"
#include "Transform.h"

CDamageFont* CDamageFont::s_pInstance = { nullptr };

CDamageFont::CDamageFont(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CGameObject{ pDevice, pContext }
{
}

CDamageFont::CDamageFont(const CDamageFont& Prototype)
    : CGameObject{ Prototype }
{
}

HRESULT CDamageFont::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CDamageFont::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    DAMAGEFONT_DESC* pDesc = static_cast<DAMAGEFONT_DESC*>(pArg);
    if (nullptr == pDesc->pTextureProtoTag)
        return E_FAIL;

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    m_strTextureProtoTag = pDesc->pTextureProtoTag;
    m_iAtlasCols = max(1u, pDesc->iAtlasCols);
    m_iAtlasRows = max(1u, pDesc->iAtlasRows);
    m_iMaxInstanceCount = max(1u, pDesc->iMaxInstanceCount);

    if (FAILED(Ready_Components(pDesc)))
        return E_FAIL;

    m_Entries.reserve(32);
    s_pInstance = this;
    return S_OK;
}

void CDamageFont::Spawn(const _float3& vWorldCenter, _int iValue, _bool bCrit)
{
    if (iValue <= 0)
        return;

    auto frand = []() { return (static_cast<_float>(rand()) / static_cast<_float>(RAND_MAX)) * 2.f - 1.f; };

    _float3 vDir{ frand(), fabsf(frand()) * 0.6f + 0.4f, frand() };
    _vector vN = XMVector3Normalize(XMLoadFloat3(&vDir));

    DAMAGE_ENTRY Entry{};
    XMStoreFloat3(&Entry.vBasePos, XMLoadFloat3(&vWorldCenter) + vN * SPHERE_RADIUS);
    Entry.iValue = iValue;
    Entry.fAge = 0.f;
    Entry.fLifetime = LIFETIME;
    Entry.bCrit = bCrit;
    m_Entries.push_back(Entry);
}

void CDamageFont::Update(_float fTimeDelta)
{
    for (auto it = m_Entries.begin(); it != m_Entries.end(); )
    {
        it->fAge += fTimeDelta;
        if (it->fAge >= it->fLifetime)
            it = m_Entries.erase(it);
        else
            ++it;
    }
}

void CDamageFont::Late_Update(_float fTimeDelta)
{
    if (m_Entries.empty())
        return;

    m_pGameInstance->Add_RenderGroup(RENDERID::BLEND, this);
}

HRESULT CDamageFont::Render()
{
    if (m_Entries.empty())
        return S_OK;

    _matrix ViewMatrix = XMLoadFloat4x4(m_pGameInstance->Get_Transform(D3DTS::VIEW));
    _matrix ViewInverse = XMMatrixInverse(nullptr, ViewMatrix);
    _vector vRightBase = XMVector3Normalize(ViewInverse.r[0]);
    _vector vUpBase = XMVector3Normalize(ViewInverse.r[1]);
    _vector vLook = XMVector3Normalize(ViewInverse.r[2]);

    vector<VTXRECT_INSTANCE> Instances;
    Instances.reserve(64);
    const _float fCellW = 1.f / static_cast<_float>(m_iAtlasCols);

    for (const auto& Entry : m_Entries)
    {
        const _float t = Entry.fAge / Entry.fLifetime;          // 0~1

        // 위치: 빠르게 솟았다가 감속 (ease-out)
        const _float fRise = RISE_DIST * (1.f - (1.f - t) * (1.f - t));

        // 팝 스케일: 등장 순간 작게 → 오버슈트 → 정착 (바운스)
        _float fPop;
        const _float pp = Entry.fAge / POP_TIME;
        if (pp >= 1.f)        fPop = 1.f;
        else if (pp < 0.6f)   fPop = 0.3f + (POP_SCALE_MAX - 0.3f) * (pp / 0.6f);
        else                  fPop = POP_SCALE_MAX + (1.f - POP_SCALE_MAX) * ((pp - 0.6f) / 0.4f);

        // fade: 후반 30%
        const _float fAlpha = (t < 0.7f) ? 1.f : max(0.f, 1.f - (t - 0.7f) / 0.3f);

        _float3 vPos = Entry.vBasePos;
        vPos.y += fRise;
        _vector vCenter = XMLoadFloat3(&vPos);

        const _float fCrit = Entry.bCrit ? CRIT_SCALE : 1.f;
        const _float fSizeX = DIGIT_SIZE_X * fPop * fCrit;
        const _float fSizeY = DIGIT_SIZE_Y * fPop * fCrit;
        const _float fSpacing = DIGIT_SPACING * fPop * fCrit;

        _vector vRight = vRightBase * fSizeX;
        _vector vUp = vUpBase * fSizeY;

        _int aDigits[8]; _int n = 0; _int v = Entry.iValue;
        if (v <= 0) aDigits[n++] = 0;
        while (v > 0 && n < 8) { aDigits[n++] = v % 10; v /= 10; }

        for (_int j = 0; j < n; ++j)
        {
            const _int d = aDigits[n - 1 - j];   // 왼쪽=높은 자리
            const _float fOff = (static_cast<_float>(j) - (n - 1) * 0.5f) * fSpacing;
            _vector vDigitPos = vCenter + vRightBase * fOff;

            VTXRECT_INSTANCE Inst{};
            XMStoreFloat4(&Inst.vRight, vRight);
            XMStoreFloat4(&Inst.vUp, vUp);
            XMStoreFloat4(&Inst.vLook, vLook);
            XMStoreFloat4(&Inst.vTranslation, vDigitPos); Inst.vTranslation.w = 1.f;
            Inst.vTexInfo = _float4(d * fCellW, 0.f, fCellW, 1.f);
            Inst.vColor = Entry.bCrit ? _float4(1.f, 0.82f, 0.2f, fAlpha)  
                                        : _float4(1.f, 1.f, 1.f, fAlpha);
            Instances.push_back(Inst);
        }
    }

    if (Instances.empty())
        return S_OK;
    if (static_cast<_uint>(Instances.size()) > m_iMaxInstanceCount)
        Instances.resize(m_iMaxInstanceCount);

    if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
        return E_FAIL;
    if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
        return E_FAIL;
    if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
        return E_FAIL;
    if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
        return E_FAIL;

    const _float fOne = 1.f;
    if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &fOne, sizeof(_float))))
        return E_FAIL;

    if (FAILED(m_pVIBufferCom->Set_Instances(Instances)))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Begin(0)))   // pass0 = AlphaBlend
        return E_FAIL;
    if (FAILED(m_pVIBufferCom->Bind_Resources()))
        return E_FAIL;
    if (FAILED(m_pVIBufferCom->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT CDamageFont::Ready_Components(const DAMAGEFONT_DESC* pDesc)
{
    if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_Shader_VtxRectInstance"), TEXT("Com_Shader"),
        reinterpret_cast<CComponent**>(&m_pShaderCom))))
        return E_FAIL;

    if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
        m_strTextureProtoTag.c_str(), TEXT("Com_Texture"),
        reinterpret_cast<CComponent**>(&m_pTextureCom))))
        return E_FAIL;

    CVIBuffer_Rect_Instance::RECT_INSTANCE_DESC BufferDesc{};
    BufferDesc.iMaxInstanceCount = m_iMaxInstanceCount;

    if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
        TEXT("Prototype_Component_VIBuffer_Rect_Instance"), TEXT("Com_VIBuffer"),
        reinterpret_cast<CComponent**>(&m_pVIBufferCom), &BufferDesc)))
        return E_FAIL;

    return S_OK;
}

CDamageFont* CDamageFont::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CDamageFont* pInstance = new CDamageFont(pDevice, pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CDamageFont");
        Safe_Release(pInstance);
    }
    return pInstance;
}

CGameObject* CDamageFont::Clone(void* pArg)
{
    CDamageFont* pInstance = new CDamageFont(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CDamageFont");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CDamageFont::Free()
{
    __super::Free();

    if (s_pInstance == this)
        s_pInstance = nullptr;

    Safe_Release(m_pVIBufferCom);
    Safe_Release(m_pTextureCom);
    Safe_Release(m_pShaderCom);
}