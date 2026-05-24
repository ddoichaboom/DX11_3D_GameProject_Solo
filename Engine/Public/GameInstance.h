#pragma once

#include "Prototype_Manager.h"

NS_BEGIN(Engine)

class ENGINE_DLL CGameInstance final : public CBase
{
	DECLARE_SINGLETON(CGameInstance)

private:
	CGameInstance();
	virtual ~CGameInstance() = default;

#pragma region ENGINE
public:
	HRESULT						Initialize_Engine(const ENGINE_DESC& EngineDesc,
													ID3D11Device** ppDevice,
													ID3D11DeviceContext** ppContext);
	void						Update_Engine(_float fTimeDelta);
	HRESULT						Begin_Draw();
	HRESULT						Draw();
	HRESULT						End_Draw();
	void 						Clear_Resources(_int iLevelIndex);

	HWND						Get_hWnd() const { return m_hWnd; }
	_uint						Get_WinSizeX() const { return m_iWinSizeX; }
	_uint						Get_WinSizeY() const { return m_iWinSizeY; }

	HRESULT						OnResize(_uint iWinSizeX, _uint iWinSizeY);
	WINMODE						Get_WinMode() const { return m_eWinMode; }
	void						Set_WinMode(WINMODE eMode) { m_eWinMode = eMode; }

	_float						Random(_float fMin, _float fMax);

#pragma endregion

#pragma region TIMER_MANAGER
public:
	_float						Get_TimeDelta(const _wstring& strTimerTag);
	HRESULT						Add_Timer(const _wstring& strTimerTag);
	void						Compute_Timer(const _wstring& strTimerTag);
#pragma endregion

#pragma region LEVEL_MANAGER
	HRESULT 					Change_Level(_int iNewLevelIndex, class CLevel* pNewLevel);
	_int						Get_CurrentLevelIndex() const;
#pragma endregion

#pragma region PROTOTYPE_MANAGER
	HRESULT						Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, CBase* pPrototype);
	CBase*						Clone_Prototype(PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg = nullptr); 
	HRESULT						Enum_Prototypes(_uint iLevelIndex, vector<PROTOTYPE_INFO>& out) const;
#pragma endregion

#pragma region OBJECT_MANAGER
	HRESULT						Add_GameObject(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag,
												_uint iLayerLevelIndex, const _wstring& strLayerTag, void* pArg = nullptr);
	const map<const _wstring, class CLayer*>* Get_Layers(_uint iLevelIndex) const;
	HRESULT						Move_GameObject(_uint iSrcLevel, const _wstring& strSrcLayer,
												_uint iDstLevel, const _wstring& strDstLayer,
												class CGameObject* pObject);
	HRESULT						Reorder_GameObject(_uint iLevel, const _wstring& strLayer,
												class CGameObject* pObject, _uint iNewIndex);
	HRESULT						Remove_GameObject(_uint iLevel, const _wstring& strLayer,
												class CGameObject* pObject);
#pragma endregion

#pragma region RENDERER
	void						Add_RenderGroup(RENDERID eGroupID, class CGameObject* pGameObject);
#pragma endregion

#pragma region TARGET_MANAGER
public:
	HRESULT						Add_RenderTarget(const _wstring& strTargetTag,
													_uint iWidth, _uint iHeight,
													DXGI_FORMAT ePixelFormat,
													const _float4& vClearColor);

	HRESULT                     Add_MRT(const _wstring& strMRTTag, const _wstring& strTargetTag);
	HRESULT                     Begin_MRT(const _wstring& strMRTTag, ID3D11DepthStencilView* pDSV = nullptr);
	HRESULT                     End_MRT();
	HRESULT                     Bind_RT_ShaderResource(const _wstring& strTargetTag,
														class CShader* pShader,
														const _char* pConstantName);
	HRESULT						Begin_ViewportRT(_uint iWidth, _uint iHeight);
	HRESULT						End_ViewportRT();
	ID3D11ShaderResourceView*	Get_ViewportSRV();

	HRESULT						Resize_RenderTargets(_uint iWidth, _uint iHeight);


#ifdef _DEBUG
	HRESULT						Ready_RT_Debug(const _wstring& strTargetTag,
												_float fX, _float fY,
												_float fSizeX, _float fSizeY,
												_float fCanvasWidth, _float fCanvasHeight);

	HRESULT                     Render_RT_Debug(const _wstring& strMRTTag,
												class CShader* pShader,
												class CVIBuffer_Rect* pVIBuffer);
#endif
#pragma endregion

#pragma region COLLISION_MANAGER
public:
	void						Add_Collider(COLLISION_GROUP eGroup, class CCollider* pCollider);
	void						Set_CollisionMatrix(COLLISION_GROUP eA, COLLISION_GROUP eB, _bool bValue);
	void						Set_Debug_Colliders(_bool bValue);
	_bool						Is_Debug_Colliders() const;
#pragma endregion


#pragma region PIPELINE
	const _float4x4*			Get_Transform(D3DTS eState) const;
	const _float4x4*			Get_Transform_Inverse(D3DTS eState) const;
	const _float4*				Get_CamPosition() const;
	void                        Compute_WorldRay(_float fViewportX, _float fViewportY,
													_float fViewportWidth, _float fViewportHeight,
													_float4* pRayOrigin, _float4* pRayDir);
	void						Set_Transform(D3DTS eState, _fmatrix StateMatrix);

	void						Transform_Frustum_ToLocalSpace(_fmatrix WorldMatrix);
	_bool                       Is_In_Frustum_WorldSpace(_fvector vWorldPos, _float fRange = 0.f) const;
	_bool                       Is_In_Frustum_LocalSpace(_fvector vLocalPos, _float fRange = 0.f) const;
#pragma endregion

#pragma region INPUT_DEVICE
	_byte						Get_KeyState(_ubyte byKeyID);
	_bool						Get_KeyDown(_ubyte byKeyID);
	_bool						Get_KeyUp(_ubyte byKeyID);
	_byte						Get_MouseBtnState(MOUSEBTN eBtn);
	_bool						Get_MouseBtnDown(MOUSEBTN eBtn);
	_bool						Get_MouseBtnUp(MOUSEBTN eBtn);
	_long						Get_MouseDelta(MOUSEAXIS eAxis);
	static void					Process_RawInput(LPARAM lParam);

	void						Set_CursorLocked(_bool bLock);
	_bool						Is_CursorLocked() const;
#pragma endregion

#pragma region GAME_LOGIC
	void					Set_GameLogic_Frozen(_bool bFrozen) { m_bLogicFrozen = bFrozen; }
	_bool					Is_GameLogic_Frozen() const { return m_bLogicFrozen; }
#pragma endregion

#pragma region LIGHT_MANAGER
	const LIGHT_DESC*			Get_LightDesc(_uint iIndex);
	HRESULT						Add_Light(const LIGHT_DESC& LightDesc);
	HRESULT                     Render_Light(class CShader* pShader, class CVIBuffer_Rect* pVIBuffer);
	_uint						Get_NumLights() const;
#pragma endregion

#pragma region Font_MANAGER
	void                        Get_FontTags(vector<_wstring>* pOut);
	HRESULT						Add_Font(const _wstring& strFontTag, const _tchar* pFontFilePath);
	HRESULT						Render_Font(const _wstring& strFontTag, const _tchar* pText, const _float2& vPosition,
												_fvector vColor = XMVectorSet(1.f, 1.f, 1.f, 1.f),
												_float fRotation = 0.f,
												const _float2& vOrigin = _float2(0.f, 0.f),
												const _float2& vScale = _float2(1.f, 1.f));
	HRESULT                     Measure_Font(const _wstring& strFontTag, const _tchar* pText, _float2* pOutSize);
#pragma endregion

private:
	class CGraphic_Device*		m_pGraphic_Device = { nullptr };
	class CTimer_Manager*		m_pTimer_Manager = { nullptr };
	class CLevel_Manager*		m_pLevel_Manager = { nullptr };
	class CPrototype_Manager*	m_pPrototype_Manager = { nullptr };
	class CObject_Manager*		m_pObject_Manager = { nullptr };
	class CRenderer*			m_pRenderer = { nullptr };
	class CPipeLine*			m_pPipeLine = { nullptr };
	class CInput_Device*		m_pInput_Device = { nullptr };
	class CLight_Manager*		m_pLight_Manager = { nullptr };
	class CFont_Manager*		m_pFont_Manager = { nullptr };
	class CCollision_Manager*	m_pCollision_Manager = { nullptr };
	class CFrustum*				m_pFrustum = { nullptr };
	class CTarget_Manager*		m_pTarget_Manager = { nullptr };

private:
	_bool						m_bLogicFrozen = { false };
	HWND						m_hWnd;
	_uint						m_iWinSizeX;
	_uint						m_iWinSizeY;
	WINMODE						m_eWinMode = { WINMODE::WIN };

public:
	void						Release_Engine();
	virtual void				Free() override;
};

NS_END