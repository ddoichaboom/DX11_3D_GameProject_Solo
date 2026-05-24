#include "NavigationAgent.h"
#include "NavMesh.h"

CNavigationAgent::CNavigationAgent(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CComponent { pDevice, pContext }
{
}

CNavigationAgent::CNavigationAgent(const CNavigationAgent& Prototype)
	: CComponent{ Prototype }
{
}

HRESULT CNavigationAgent::Initialize_Prototype()
{
	return S_OK;
}

HRESULT CNavigationAgent::Initialize(void* pArg)
{
    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    if (nullptr != pArg)
    {
        NAVIGATION_AGENT_DESC* pDesc = static_cast<NAVIGATION_AGENT_DESC*>(pArg);

        Bind_NavMesh(pDesc->pNavMesh);
        m_iCurrentCellIndex = pDesc->iStartCellIndex;
    }

    return S_OK;
}

void CNavigationAgent::Bind_NavMesh(CNavMesh* pNavMesh)
{
    if (m_pNavMesh == pNavMesh)
        return;

    Safe_Release(m_pNavMesh);

    m_pNavMesh = pNavMesh;
    Safe_AddRef(m_pNavMesh);

    m_iCurrentCellIndex = INVALID_INDEX;
}

void CNavigationAgent::UnBind_NavMesh()
{
    Safe_Release(m_pNavMesh);
    m_iCurrentCellIndex = INVALID_INDEX;
}

_bool CNavigationAgent::Try_Move(const _float3& vCandidatePosition, _float3* pOutAdjustedPosition)
{
    if (nullptr == m_pNavMesh)
        return false;

    return m_pNavMesh->Try_Move(&m_iCurrentCellIndex, vCandidatePosition, pOutAdjustedPosition);
}

_bool CNavigationAgent::Find_CurrentCell(const _float3& vPosition)
{
    if (nullptr == m_pNavMesh)
        return false;

    m_iCurrentCellIndex = m_pNavMesh->Find_Cell(vPosition);

    return INVALID_INDEX != m_iCurrentCellIndex;
}

CNavigationAgent* CNavigationAgent::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CNavigationAgent* pInstance = new CNavigationAgent(pDevice, pContext);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : CNavigationAgent");
        Safe_Release(pInstance);
    }

    return pInstance;
}

CComponent* CNavigationAgent::Clone(void* pArg)
{
    CNavigationAgent* pInstance = new CNavigationAgent(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : CNavigationAgent");
        Safe_Release(pInstance);
    }

    return pInstance;
}

void CNavigationAgent::Free()
{
    __super::Free();

    Safe_Release(m_pNavMesh);
    m_iCurrentCellIndex = INVALID_INDEX;
}
