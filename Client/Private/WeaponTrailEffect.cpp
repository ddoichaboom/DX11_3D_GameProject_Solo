#include "WeaponTrailEffect.h"
#include "GameInstance.h"
#include "Shader.h"
#include "Texture.h"

const D3D11_INPUT_ELEMENT_DESC CWeaponTrailEffect::VTXTRAIL::Elements[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

CWeaponTrailEffect::CWeaponTrailEffect(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CGameObject{ pDevice, pContext }
{
}

CWeaponTrailEffect::CWeaponTrailEffect(const CWeaponTrailEffect& Prototype)
	: CGameObject{ Prototype }
{
}

HRESULT CWeaponTrailEffect::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CWeaponTrailEffect::Initialize(void* pArg)
{
	if (nullptr == pArg)
		return E_FAIL;

	WEAPON_TRAIL_EFFECT_DESC* pDesc = static_cast<WEAPON_TRAIL_EFFECT_DESC*>(pArg);

	if (nullptr == pDesc->pStartBoneMatrix ||
		nullptr == pDesc->pEndBoneMatrix ||
		nullptr == pDesc->pParentWorldMatrix ||
		nullptr == pDesc->pTexturePrototypeTag)
		return E_FAIL;

	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	m_pStartBoneMatrix = pDesc->pStartBoneMatrix;
	m_pEndBoneMatrix = pDesc->pEndBoneMatrix;
	m_pParentWorldMatrix = pDesc->pParentWorldMatrix;
	m_strTexturePrototypeTag = pDesc->pTexturePrototypeTag;
	m_vColor = pDesc->vColor;
	m_iMaxSamples = max(2u, pDesc->iMaxSamples);
	m_iMaxVertices = m_iMaxSamples * 2u;
	m_fSampleInterval = max(0.001f, pDesc->fSampleInterval);
	m_fLifeTime = max(0.001f, pDesc->fLifeTime);
	m_fMinSampleDistance = max(0.f, pDesc->fMinSampleDistance);
	m_bActive = pDesc->bInitiallyActive;

	m_Samples.reserve(m_iMaxSamples);
	m_Vertices.reserve(m_iMaxVertices);

	if (FAILED(Ready_Components()))
		return E_FAIL;

	if (FAILED(Ready_Buffers()))
		return E_FAIL;

	return S_OK;
}

void CWeaponTrailEffect::Update(_float fTimeDelta)
{
	for (auto& Sample : m_Samples)
		Sample.fAge += fTimeDelta;

	m_Samples.erase(
		remove_if(m_Samples.begin(), m_Samples.end(),
			[this](const TRAIL_SAMPLE& Sample)
			{
				return Sample.fAge >= m_fLifeTime;
			}),
		m_Samples.end());

	if (false == m_bActive)
		return;

	m_fSampleTimer += fTimeDelta;

	if (m_Samples.empty() || m_fSampleTimer >= m_fSampleInterval)
	{
		m_fSampleTimer = 0.f;
		Sample_CurrentPose();
	}
}

void CWeaponTrailEffect::Late_Update(_float fTimeDelta)
{
	if (m_Samples.size() < 2)
		return;

	m_pGameInstance->Add_RenderGroup(RENDERID::BLEND, this);
}

HRESULT CWeaponTrailEffect::Render()
{
	Build_Vertices(&m_Vertices);

	if (m_Vertices.size() < 4)
		return S_OK;

	D3D11_MAPPED_SUBRESOURCE Mapped{};
	if (FAILED(m_pContext->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		return E_FAIL;

	memcpy(Mapped.pData, m_Vertices.data(), sizeof(VTXTRAIL) * m_Vertices.size());
	m_pContext->Unmap(m_pVB, 0);

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Begin(0)))
		return E_FAIL;

	ID3D11Buffer* pVertexBuffers[] = { m_pVB };
	_uint iStrides[] = { sizeof(VTXTRAIL) };
	_uint iOffsets[] = { 0 };

	m_pContext->IASetVertexBuffers(0, 1, pVertexBuffers, iStrides, iOffsets);
	m_pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->Draw(static_cast<_uint>(m_Vertices.size()), 0);

	return S_OK;
}

void CWeaponTrailEffect::Set_Active(_bool bActive)
{
	if (m_bActive == bActive)
		return;

	m_bActive = bActive;
	m_fSampleTimer = m_fSampleInterval;

	if (true == m_bActive && true == m_Samples.empty())
		Sample_CurrentPose();
}

void CWeaponTrailEffect::Reset()
{
	m_Samples.clear();
	m_Vertices.clear();
	m_fSampleTimer = 0.f;
}

HRESULT CWeaponTrailEffect::Ready_Components()
{
	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		TEXT("Prototype_Component_Shader_WeaponTrail"),
		TEXT("Com_Shader"),
		reinterpret_cast<CComponent**>(&m_pShaderCom))))
		return E_FAIL;

	if (FAILED(Add_Component(ETOUI(LEVEL::GAMEPLAY),
		m_strTexturePrototypeTag,
		TEXT("Com_Texture"),
		reinterpret_cast<CComponent**>(&m_pTextureCom))))
		return E_FAIL;

	return S_OK;
}

HRESULT CWeaponTrailEffect::Ready_Buffers()
{
	D3D11_BUFFER_DESC BufferDesc{};
	BufferDesc.ByteWidth = sizeof(VTXTRAIL) * m_iMaxVertices;
	BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&BufferDesc, nullptr, &m_pVB)))
		return E_FAIL;

	return S_OK;
}

HRESULT CWeaponTrailEffect::Bind_ShaderResources()
{
	_float4x4 Identity{};
	XMStoreFloat4x4(&Identity, XMMatrixIdentity());

	if (FAILED(m_pShaderCom->Bind_Matrix("g_WorldMatrix", &Identity)))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ViewMatrix", m_pGameInstance->Get_Transform(D3DTS::VIEW))))
		return E_FAIL;

	if (FAILED(m_pShaderCom->Bind_Matrix("g_ProjMatrix", m_pGameInstance->Get_Transform(D3DTS::PROJ))))
		return E_FAIL;

	if (FAILED(m_pTextureCom->Bind_ShaderResource(m_pShaderCom, "g_Texture", 0)))
		return E_FAIL;

	return S_OK;
}

void CWeaponTrailEffect::Sample_CurrentPose()
{
	_float3 vStart{};
	_float3 vEnd{};

	if (false == Compute_CurrentPoints(&vStart, &vEnd))
		return;

	if (false == Should_AddSample(vStart, vEnd))
		return;

	TRAIL_SAMPLE Sample{};
	Sample.vStart = vStart;
	Sample.vEnd = vEnd;
	Sample.fAge = 0.f;

	m_Samples.push_back(Sample);

	while (m_Samples.size() > m_iMaxSamples)
		m_Samples.erase(m_Samples.begin());
}

_bool CWeaponTrailEffect::Compute_CurrentPoints(_float3* pOutStart, _float3* pOutEnd) const
{
	if (nullptr == pOutStart || nullptr == pOutEnd ||
		nullptr == m_pStartBoneMatrix ||
		nullptr == m_pEndBoneMatrix ||
		nullptr == m_pParentWorldMatrix)
		return false;

	_matrix ParentMatrix = XMLoadFloat4x4(m_pParentWorldMatrix);
	_matrix StartMatrix = XMLoadFloat4x4(m_pStartBoneMatrix) * ParentMatrix;
	_matrix EndMatrix = XMLoadFloat4x4(m_pEndBoneMatrix) * ParentMatrix;

	XMStoreFloat3(pOutStart, StartMatrix.r[3]);
	XMStoreFloat3(pOutEnd, EndMatrix.r[3]);

	return true;
}

_bool CWeaponTrailEffect::Should_AddSample(const _float3& vStart, const _float3& vEnd) const
{
	if (true == m_Samples.empty())
		return true;

	const TRAIL_SAMPLE& Last = m_Samples.back();
	_vector vLastStart = XMLoadFloat3(&Last.vStart);
	_vector vLastEnd = XMLoadFloat3(&Last.vEnd);
	_vector vCurStart = XMLoadFloat3(&vStart);
	_vector vCurEnd = XMLoadFloat3(&vEnd);

	const _float fMinDistSq = m_fMinSampleDistance * m_fMinSampleDistance;

	return XMVectorGetX(XMVector3LengthSq(vCurStart - vLastStart)) >= fMinDistSq ||
		XMVectorGetX(XMVector3LengthSq(vCurEnd - vLastEnd)) >= fMinDistSq;
}

void CWeaponTrailEffect::Build_Vertices(vector<VTXTRAIL>* pOutVertices) const
{
	if (nullptr == pOutVertices)
		return;

	pOutVertices->clear();

	if (m_Samples.size() < 2)
		return;

	const _uint iNumSamples = static_cast<_uint>(m_Samples.size());
	pOutVertices->reserve(iNumSamples * 2u);

	for (_uint i = 0; i < iNumSamples; ++i)
	{
		const TRAIL_SAMPLE& Sample = m_Samples[i];
		const _float fU = (iNumSamples > 1) ? static_cast<_float>(i) / static_cast<_float>(iNumSamples - 1u) : 0.f;
		const _float fAgeAlpha = max(0.f, 1.f - (Sample.fAge / m_fLifeTime));
		_float4 vColor = m_vColor;
		vColor.w *= fAgeAlpha;

		VTXTRAIL StartVertex{};
		StartVertex.vPosition = Sample.vStart;
		StartVertex.vTexcoord = _float2(fU, 0.f);
		StartVertex.vColor = vColor;

		VTXTRAIL EndVertex{};
		EndVertex.vPosition = Sample.vEnd;
		EndVertex.vTexcoord = _float2(fU, 1.f);
		EndVertex.vColor = vColor;

		pOutVertices->push_back(StartVertex);
		pOutVertices->push_back(EndVertex);
	}
}

CWeaponTrailEffect* CWeaponTrailEffect::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CWeaponTrailEffect* pInstance = new CWeaponTrailEffect(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : CWeaponTrailEffect");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CGameObject* CWeaponTrailEffect::Clone(void* pArg)
{
	CWeaponTrailEffect* pInstance = new CWeaponTrailEffect(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CWeaponTrailEffect");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CWeaponTrailEffect::Free()
{
	__super::Free();

	Safe_Release(m_pVB);
	Safe_Release(m_pTextureCom);
	Safe_Release(m_pShaderCom);
}
