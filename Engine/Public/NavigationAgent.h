#pragma once

#include "Component.h"
#include "NavMesh_Types.h"

NS_BEGIN(Engine)

class CNavMesh;

class ENGINE_DLL CNavigationAgent final : public CComponent
{
public:
	typedef struct tagNavigationAgentDesc
	{
		CNavMesh* pNavMesh = { nullptr };
		_int iStartCellIndex = { INVALID_INDEX };
	}NAVIGATION_AGENT_DESC;

private:
	CNavigationAgent(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CNavigationAgent(const CNavigationAgent& Prototype);
	virtual ~CNavigationAgent() = default;

public:
	virtual HRESULT				Initialize_Prototype() override;
	virtual HRESULT				Initialize(void* pArg) override;

public:
	void						Bind_NavMesh(CNavMesh* pNavMesh);
	void						UnBind_NavMesh();

	_bool						Has_NavMesh() const { return nullptr != m_pNavMesh; }
	CNavMesh*					Get_NavMesh() const { return m_pNavMesh; }

	_int						Get_CurrentCellIndex() const { return m_iCurrentCellIndex; }
	void						Set_CurrentCellIndex(_int iCellIndex) { m_iCurrentCellIndex = iCellIndex; }

	_bool						Try_Move(const _float3& vCandidatePosition, _float3* pOutAdjustedPosition);
	_bool						Find_CurrentCell(const _float3& vPosition);


private:
	CNavMesh*					m_pNavMesh = { nullptr };
	_int						m_iCurrentCellIndex = { INVALID_INDEX };

public:
	static CNavigationAgent*	Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual CComponent*			Clone(void* pArg) override;
	virtual void				Free() override;
};

NS_END
