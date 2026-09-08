#include "UI_SpriteAnim.h"
#include "GameInstance.h"
#include "Shader.h"

CUI_SpriteAnim::CUI_SpriteAnim(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CUI_Image{ pDevice, pContext }
{
}

CUI_SpriteAnim::CUI_SpriteAnim(const CUI_SpriteAnim& Prototype)
    : CUI_Image{ Prototype }
{
}

HRESULT CUI_SpriteAnim::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT CUI_SpriteAnim::Initialize(void* pArg)
{
    if (nullptr == pArg)
        return E_FAIL;

    auto pDesc = static_cast<UI_SPRITEANIM_DESC*>(pArg);

    m_iAtlasCols = max(1u, pDesc->iAtlasCols);
    m_iAtlasRows = max(1u, pDesc->iAtlasRows);
    m_fFrameDuration = max(0.001f, pDesc->fFrameDuration);
    m_bLoop = pDesc->bLoop;

    // 부모 CUI_Image::Initialize 가 UIOBJECT_DESC 기반 transform + Ready_Components 처리
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    return S_OK;
}

void CUI_SpriteAnim::Update(_float fTimeDelta)
{
    if (m_bFinished)
        return;

    m_fFrameTimer += fTimeDelta;
    while (m_fFrameTimer >= m_fFrameDuration)
    {
        m_fFrameTimer -= m_fFrameDuration;
        ++m_iCurFrame;

        const _uint iTotal = m_iAtlasCols * m_iAtlasRows;
        if (m_iCurFrame >= iTotal)
        {
            if (m_bLoop)
                m_iCurFrame = 0;
            else
            {
                m_iCurFrame = iTotal - 1;
                m_bFinished = true;
                break;
            }
        }
    }
}

HRESULT CUI_SpriteAnim::Render()
{
    if (FAILED(Bind_SpriteAnim_Resources()))
        return E_FAIL;

    if (FAILED(m_pShaderCom->Begin(2)))     // SpriteAnimPass
        return E_FAIL;

    if (FAILED(m_pVIBufferCom->Bind_Resources()))
        return E_FAIL;

    if (FAILED(m_pVIBufferCom->Render()))
        return E_FAIL;

    return S_OK;
}

HRESULT CUI_SpriteAnim::Bind_SpriteAnim_Resources()
{
    if (FAILED(__super::Bind_ShaderResources()))    // World/View/Proj + Texture
        return E_FAIL;

    _float4 vUVOffsetScale{};
    Compute_UVOffsetScale(&vUVOffsetScale);

    if (FAILED(m_pShaderCom->Bind_RawValue("g_vUVOffsetScale", &vUVOffsetScale, sizeof(_float4))))
        return E_FAIL;

    const _float fOne = 1.f;
    if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &fOne, sizeof(_float)))) return E_FAIL;
    if (FAILED(m_pShaderCom->Bind_RawValue("g_fGaugeProgress", &fOne, sizeof(_float)))) return E_FAIL;


    return S_OK;
}

void CUI_SpriteAnim::Compute_UVOffsetScale(_float4* pOut) const
{
    if (nullptr == pOut)
        return;

    const _float fFrameW = 1.f / static_cast<_float>(m_iAtlasCols);
    const _float fFrameH = 1.f / static_cast<_float>(m_iAtlasRows);

    const _uint iCol = m_iCurFrame % m_iAtlasCols;
    const _uint iRow = m_iCurFrame / m_iAtlasCols;

    pOut->x = iCol * fFrameW;   // offset.x
    pOut->y = iRow * fFrameH;   // offset.y
    pOut->z = fFrameW;          // scale.x
    pOut->w = fFrameH;          // scale.y
}

CUI_SpriteAnim* CUI_SpriteAnim::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CUI_SpriteAnim* pInstance = new CUI_SpriteAnim(pDevice, pContext);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CUI_SpriteAnim");
        Safe_Release(pInstance);
    }
    return pInstance;
}

CGameObject* CUI_SpriteAnim::Clone(void* pArg)
{
    CUI_SpriteAnim* pInstance = new CUI_SpriteAnim(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CUI_SpriteAnim");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CUI_SpriteAnim::Free()
{
    __super::Free();
}