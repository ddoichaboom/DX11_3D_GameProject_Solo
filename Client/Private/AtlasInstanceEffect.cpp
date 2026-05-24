#include "AtlasInstanceEffect.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect_Instance.h"

CAtlasInstanceEffect::CAtlasInstanceEffect(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CAtlasInstanceEffect::CAtlasInstanceEffect(const CAtlasInstanceEffect& Prototype)
	: CGameObject{ Prototype }
{
}

HRESULT CAtlasInstanceEffect::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CAtlasInstanceEffect::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	ATLAS_INSTANCE_EFFECT_DESC* pDesc = static_cast<ATLAS_INSTANCE_EFFECT_DESC*>(pArg);

	if (nullptr == pDesc->pTexturePrototypeTag || nullptr == pDesc->pPositions)
		return E_FAIL;

	if (pDesc->pPositions->empty())
		return E_FAIL;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_strTexturePrototypeTag = pDesc->pTexturePrototypeTag;
	m_Positions = *pDesc->pPositions;
	m_iAtlasCols = max(1u, pDesc->iAtlasCols);
	m_iAtlasRows = max(1u, pDesc->iAtlasRows);
	m_fFrameDuration = max(0.001f, pDesc->fFrameDuration);
	m_vSize = pDesc->vSize;
	m_vUVPadding = pDesc->vUVPadding;
	m_vColor = pDesc->vColor;
	m_fAlpha = pDesc->fAlpha;
	m_bLoop = pDesc->bLoop;

	if (FAILED(Ready_Components(pDesc)))
		return E_FAIL;

	return S_OK;
}

void CAtlasInstanceEffect::Update(_float fTimeDelta)
{
	Advance_Frame(fTimeDelta);
}

void CAtlasInstanceEffect::Late_Update(_float fTimeDelta)
{
	if (true == m_bFinished)
		return;

	if (true == m_Positions.empty())
		return;

	m_pGameInstance->Add_RenderGroup(RENDERID::BLEND, this);
}

HRESULT CAtlasInstanceEffect::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Begin(0)))
		return E_FAIL;

	if (FAILED(m_pVIBufferInstanceCom->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_pVIBufferInstanceCom->Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT CAtlasInstanceEffect::Ready_Components(const ATLAS_INSTANCE_EFFECT_DESC* pDesc)
{
	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_Shader_VtxRectInstance"),
		TEXT("Com_Shader"),
		reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		m_strTexturePrototypeTag,
		TEXT("Com_Texture"),
		reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	CVIBuffer_Rect_Instance::RECT_INSTANCE_DESC BufferDesc{};
	BufferDesc.iMaxInstanceCount = max(pDesc->iMaxInstanceCount, static_cast<_uint>(m_Positions.size()));

	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_VIBuffer_Rect_Instance"),
		TEXT("Com_VIBuffer"),
		reinterpret_cast<CComponent**>(&m_pVIBufferInstanceCom),
		&BufferDesc)))
		return E_FAIL;

	return S_OK;
}

HRESULT CAtlasInstanceEffect::Bind_ShaderResources()
{
	if (FAILED(m_pTransformCom->Bind_ShaderResource(m_pShaderCom, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_RawValue("g_fAlpha", &m_fAlpha, sizeof(_float))))
		return E_FAIL;

	_float4 vTexInfo = Compute_TexInfo();
	_matrix ViewMatrix = XMLoadFloat4x4(m_pGameInstance->Get_Transform(D3DTS::VIEW));

	if (FAILED(m_pVIBufferInstanceCom->Update_Billboard(m_Positions, m_vSize, vTexInfo, m_vColor, ViewMatrix)))
		return E_FAIL;

	return S_OK;
}

_float4 CAtlasInstanceEffect::Compute_TexInfo() const
{
	const _uint iTotalFrames = max(1u, m_iAtlasCols * m_iAtlasRows);
	const _uint iFrame = min(m_iCurFrame, iTotalFrames - 1);
	const _uint iCol = iFrame % m_iAtlasCols;
	const _uint iRow = iFrame / m_iAtlasCols;

	const _float fCellWidth = 1.f / static_cast<_float>(m_iAtlasCols);
	const _float fCellHeight = 1.f / static_cast<_float>(m_iAtlasRows);

	const _float fOffsetX = iCol * fCellWidth + m_vUVPadding.x;
	const _float fOffsetY = iRow * fCellHeight + m_vUVPadding.y;
	const _float fScaleX = max(0.f, fCellWidth - m_vUVPadding.x * 2.f);
	const _float fScaleY = max(0.f, fCellHeight - m_vUVPadding.y * 2.f);

	return _float4(fOffsetX, fOffsetY, fScaleX, fScaleY);
}

void CAtlasInstanceEffect::Advance_Frame(_float fTimeDelta)
{
	if (true == m_bFinished)
		return;

	m_fFrameTimer += fTimeDelta;

	const _uint iTotalFrames = max(1u, m_iAtlasCols * m_iAtlasRows);

	while (m_fFrameTimer >= m_fFrameDuration)
	{
		m_fFrameTimer -= m_fFrameDuration;
		++m_iCurFrame;

		if (m_iCurFrame < iTotalFrames)
			continue;

		if (true == m_bLoop)
		{
			m_iCurFrame = 0;
		}
		else
		{
			m_iCurFrame = iTotalFrames - 1;
			m_bFinished = true;
			break;
		}
	}
}

CAtlasInstanceEffect* CAtlasInstanceEffect::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CAtlasInstanceEffect* pInstance = new CAtlasInstanceEffect(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CAtlasInstanceEffect");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CAtlasInstanceEffect::Clone(void* pArg)
{
	CAtlasInstanceEffect* pInstance = new CAtlasInstanceEffect(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CAtlasInstanceEffect");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CAtlasInstanceEffect::Free()
{
	__super::Free();

	Safe_Release(m_pVIBufferInstanceCom);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pShaderCom);
}
