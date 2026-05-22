#include "Light_Manager.h"
#include "Light.h"

CLight_Manager::CLight_Manager(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice{ pDevice }
	, m_pContext{ pContext }
{
   Safe_AddRef(m_pDevice);
   Safe_AddRef(m_pContext);
}

const LIGHT_DESC* CLight_Manager::Get_LightDesc(_uint iIndex)
{
    // (1) 인덱스 범위 검사
    if (iIndex >= m_Lights.size())
        return nullptr;

    // (2) List 순회로 iIndex 번째 요소에 접근
    auto    iter    =   m_Lights.begin();

    for (size_t i = 0; i < iIndex; i++)
        ++iter;

    if (iter == m_Lights.end())
        return nullptr;

    // (3)  CLight의 LIGHT_DESC 포인터 반환
    return (*iter)->Get_LightDesc();
}

HRESULT CLight_Manager::Add_Light(const LIGHT_DESC& LightDesc)
{
    // CLight  인스턴스 생성 -> 리스트에 추가
	CLight* pLight = CLight::Create(m_pDevice, m_pContext, LightDesc);

    if (nullptr == pLight)
        return E_FAIL;

	m_Lights.push_back(pLight);

    return S_OK;
}

HRESULT CLight_Manager::Render(CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
    for (auto& pLight : m_Lights)
    {
        if (nullptr != pLight)
        {
            if (FAILED(pLight->Render(pShader, pVIBuffer)))
                return E_FAIL;
        }
    }

    return S_OK;
}

CLight_Manager* CLight_Manager::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    return new CLight_Manager(pDevice, pContext);
}

void CLight_Manager::Free()
{
    __super::Free();

    for (auto& pLight : m_Lights)
        Safe_Release(pLight);

    m_Lights.clear();

    Safe_Release(m_pDevice);
    Safe_Release(m_pContext);
}
