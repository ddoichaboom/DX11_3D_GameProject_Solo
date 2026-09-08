#include "GameInstance.h"
#include "Graphic_Device.h"
#include "Timer_Manager.h"
#include "Level_Manager.h"
#include "Object_Manager.h"
#include "Renderer.h"
#include "PipeLine.h"
#include "Input_Device.h"
#include "Light_Manager.h"
#include "Font_Manager.h"
#include "Collision_Manager.h"
#include "Frustum.h"
#include "Target_Manager.h"
#include "Sound_Manager.h"
#include "Shadow.h"

IMPLEMENT_SINGLETON(CGameInstance)

CGameInstance::CGameInstance()
{
}

#pragma region ENGINE

HRESULT CGameInstance::Initialize_Engine(const ENGINE_DESC& EngineDesc, ID3D11Device** ppDevice, ID3D11DeviceContext** ppContext)
{
	// Client 내부적으로 전역 변수를 제거했으므로 GameInstance에서 접근하도록 변경
	m_hWnd = EngineDesc.hWnd;
	m_iWinSizeX = EngineDesc.iViewportWidth;
	m_iWinSizeY = EngineDesc.iViewportHeight;
	m_eWinMode = EngineDesc.eWinMode;

	m_pGraphic_Device = CGraphic_Device::Create(EngineDesc.hWnd, EngineDesc.eWinMode, EngineDesc.iViewportWidth, EngineDesc.iViewportHeight, ppDevice, ppContext);
	if (nullptr == m_pGraphic_Device)
		return E_FAIL;

	m_pTimer_Manager = CTimer_Manager::Create();
	if (nullptr == m_pTimer_Manager)
		return E_FAIL;

	m_pLevel_Manager = CLevel_Manager::Create();
	if (nullptr == m_pLevel_Manager)
		return E_FAIL;

	m_pPrototype_Manager = CPrototype_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pPrototype_Manager)
		return E_FAIL;

	m_pObject_Manager = CObject_Manager::Create(EngineDesc.iNumLevels);
	if (nullptr == m_pObject_Manager)
		return E_FAIL;

	m_pTarget_Manager = CTarget_Manager::Create(*ppDevice, *ppContext);
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	m_pRenderer = CRenderer::Create(*ppDevice, *ppContext);
	if (nullptr == m_pRenderer)
		return E_FAIL;

	m_pPipeLine = CPipeLine::Create();
	if (nullptr == m_pPipeLine)
		return E_FAIL;

	m_pShadow = CShadow::Create();
	if (nullptr == m_pShadow)
		return E_FAIL;

	m_pInput_Device = CInput_Device::Create(EngineDesc.hWnd);
	if (nullptr == m_pInput_Device)
		return E_FAIL;

	m_pLight_Manager = CLight_Manager::Create(*ppDevice, *ppContext);
	if (nullptr == m_pLight_Manager)
		return E_FAIL;

	m_pFont_Manager = CFont_Manager::Create(*ppDevice, *ppContext);
	if (nullptr == m_pFont_Manager)
		return E_FAIL;

	m_pCollision_Manager = CCollision_Manager::Create(*ppDevice, *ppContext);
	if (nullptr == m_pCollision_Manager)
		return E_FAIL;

	m_pFrustum = CFrustum::Create();
	if (nullptr == m_pFrustum)
		return E_FAIL;

	m_pSound_Manager = CSound_Manager::Create(TEXT("../../Resources/Audio"));
	if (nullptr == m_pSound_Manager)
		return E_FAIL;

	return S_OK;
}

void CGameInstance::Update_Engine(_float fTimeDelta)
{
	if (nullptr != m_pSound_Manager)
		m_pSound_Manager->Update();

	m_pInput_Device->Update();							// (1) Raw Input -> frame input copy
	m_pObject_Manager->Priority_Update(fTimeDelta);		// (2) 카메라 이동 처리
	m_pObject_Manager->Update(fTimeDelta);				// (3) 카메라 -> PipeLine 세팅

	m_pPipeLine->Update();								// (4) 역행렬 계산, 카메라 위치 추출

	m_pFrustum->Update(m_pPipeLine->Get_Transform_Inverse(D3DTS::VIEW),
						m_pPipeLine->Get_Transform_Inverse(D3DTS::PROJ));		// (5) 절두체 갱신 

	m_pObject_Manager->Late_Update(fTimeDelta);			// (6) 렌더 등록

	m_pCollision_Manager->Update();

	m_pLevel_Manager->Update(fTimeDelta);
}

HRESULT CGameInstance::Begin_Draw()
{
	// 색상 : 파란색 설정
	_float4     vColor = _float4(0.2f, 0.2f, 0.2f, 1.f);

	if (FAILED(m_pGraphic_Device->Clear_BackBuffer_View(&vColor)))
		return E_FAIL;

	if (FAILED(m_pGraphic_Device->Clear_DepthStencil_View()))
		return E_FAIL;

	return S_OK;
}

HRESULT CGameInstance::Draw()
{
	if (FAILED(m_pRenderer->Draw()))
		return E_FAIL;

#ifdef _DEBUG
	if (m_bRenderCollider)
#endif
	{
		if (FAILED(m_pCollision_Manager->Render()))
			return E_FAIL;
	}

	if (FAILED(m_pLevel_Manager->Render()))
		return E_FAIL;

	return S_OK;
}

HRESULT CGameInstance::End_Draw()
{
	m_pCollision_Manager->End_Frame();
	return m_pGraphic_Device->Present();
}

void CGameInstance::Clear_Resources(_int iLevelIndex)
{
	if (-1 == iLevelIndex)
		return;

	/*iLevelIndex용 자원을 정리한다. */
	m_pObject_Manager->Clear(iLevelIndex);

	m_pPrototype_Manager->Clear(iLevelIndex);
}

HRESULT CGameInstance::OnResize(_uint iWinSizeX, _uint iWinSizeY)
{
	if (nullptr == m_pGraphic_Device)
		return S_OK;

	// 동일 크기면 스킵
	if (m_iWinSizeX == iWinSizeX && m_iWinSizeY == iWinSizeY)
		return S_OK;

	m_iWinSizeX = iWinSizeX;
	m_iWinSizeY = iWinSizeY;

	return m_pGraphic_Device->OnResize(iWinSizeX, iWinSizeY);
}

_float CGameInstance::Random(_float fMin, _float fMax)
{
	return fMin + static_cast<_float>(rand()) / RAND_MAX * (fMax - fMin);
}

#pragma endregion

#pragma region TIMER_MANAGER

_float CGameInstance::Get_TimeDelta(const _wstring& strTimerTag)
{
	return m_pTimer_Manager->Get_TimeDelta(strTimerTag);
}

HRESULT CGameInstance::Add_Timer(const _wstring& strTimerTag)
{
	return m_pTimer_Manager->Add_Timer(strTimerTag);
}

void CGameInstance::Compute_Timer(const _wstring& strTimerTag)
{
	m_pTimer_Manager->Compute_Timer(strTimerTag);
}

#pragma endregion

#pragma region LEVEL_MANAGER

HRESULT CGameInstance::Change_Level(_int iNewLevelIndex, CLevel* pNewLevel)
{
	return m_pLevel_Manager->Change_Level(iNewLevelIndex, pNewLevel);
}

_int CGameInstance::Get_CurrentLevelIndex() const
{
	return m_pLevel_Manager->Get_CurrentLevelIndex();
}

#pragma endregion

#pragma region PROTOTYPE_MANAGER

HRESULT	CGameInstance::Add_Prototype(_uint iLevelIndex, const _wstring& strPrototypeTag, CBase* pPrototype)
{
	return m_pPrototype_Manager->Add_Prototype(iLevelIndex, strPrototypeTag, pPrototype);
}

CBase* CGameInstance::Clone_Prototype(PROTOTYPE eType, _uint iLevelIndex, const _wstring& strPrototypeTag, void* pArg)
{
	return m_pPrototype_Manager->Clone_Prototype(eType, iLevelIndex, strPrototypeTag, pArg);
}

HRESULT CGameInstance::Enum_Prototypes(_uint iLevelIndex, vector<PROTOTYPE_INFO>& out) const
{
	return m_pPrototype_Manager->Enum_Prototypes(iLevelIndex, out);
}

#pragma endregion

#pragma region OBJECT_MANAGER

HRESULT	CGameInstance::Add_GameObject(_uint iPrototypeLevelIndex, const _wstring& strPrototypeTag,
	_uint iLayerLevelIndex, const _wstring& strLayerTag, void* pArg)
{
	return m_pObject_Manager->Add_GameObject(iPrototypeLevelIndex, strPrototypeTag, iLayerLevelIndex, strLayerTag, pArg);
}

const map<const _wstring, CLayer*>* CGameInstance::Get_Layers(_uint iLevelIndex) const
{
	return m_pObject_Manager->Get_Layers(iLevelIndex);
}

HRESULT		CGameInstance::Move_GameObject(_uint iSrcLevel, const _wstring& strSrcLayer,
	_uint iDstLevel, const _wstring& strDstLayer,
	CGameObject* pObject)
{
	return m_pObject_Manager->Move_GameObject(iSrcLevel, strSrcLayer, iDstLevel, strDstLayer, pObject);
}

HRESULT		CGameInstance::Reorder_GameObject(_uint iLevel, const _wstring& strLayer,
	CGameObject* pObject, _uint iNewIndex)
{
	return m_pObject_Manager->Reorder_GameObject(iLevel, strLayer, pObject, iNewIndex);
}

HRESULT		CGameInstance::Remove_GameObject(_uint iLevel, const _wstring& strLayer,
	CGameObject* pObject)
{
	return m_pObject_Manager->Remove_GameObject(iLevel, strLayer, pObject);
}
#pragma endregion

#pragma region RENDERER

void CGameInstance::Add_RenderGroup(RENDERID eGroupID, class CGameObject* pGameObject)
{
	m_pRenderer->Add_RenderGroup(eGroupID, pGameObject);
}
#pragma endregion

#pragma region TARGET_MANAGER

HRESULT CGameInstance::Add_RenderTarget(const _wstring& strTargetTag,
	_uint iWidth, _uint iHeight, DXGI_FORMAT ePixelFormat, const _float4& vClearColor)
{
	return m_pTarget_Manager->Add_RenderTarget(strTargetTag, iWidth, iHeight, ePixelFormat, vClearColor);
}

HRESULT CGameInstance::Add_MRT(const _wstring& strMRTTag, const _wstring& strTargetTag)
{
	return m_pTarget_Manager->Add_MRT(strMRTTag, strTargetTag);
}

HRESULT CGameInstance::Begin_MRT(const _wstring& strMRTTag, ID3D11DepthStencilView* pDSV)
{
	return m_pTarget_Manager->Begin_MRT(strMRTTag, pDSV);
}

HRESULT CGameInstance::End_MRT()
{
	return m_pTarget_Manager->End_MRT();
}

HRESULT CGameInstance::Bind_RT_ShaderResource(const _wstring& strTargetTag, CShader* pShader, const _char* pConstantName)
{
	return m_pTarget_Manager->Bind_ShaderResource(strTargetTag, pShader, pConstantName);
}

HRESULT CGameInstance::Begin_ViewportRT(_uint iWidth, _uint iHeight)
{
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	return m_pTarget_Manager->Begin_ViewportRT(iWidth, iHeight);
}

HRESULT CGameInstance::End_ViewportRT()
{
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	return m_pTarget_Manager->End_ViewportRT();
}

ID3D11ShaderResourceView* CGameInstance::Get_ViewportSRV()
{
	if (nullptr == m_pTarget_Manager)
		return nullptr;

	return m_pTarget_Manager->Get_RenderTargetSRV(TEXT("Target_Viewport"));
}

HRESULT CGameInstance::Resize_RenderTargets(_uint iWidth, _uint iHeight)
{
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	if (FAILED(m_pTarget_Manager->Resize_RenderTargets(iWidth, iHeight)))
		return E_FAIL;

	if (nullptr != m_pRenderer)
	{
		if (FAILED(m_pRenderer->Resize(iWidth, iHeight)))
			return E_FAIL;

#ifdef _DEBUG
		if (FAILED(m_pRenderer->Resize_DebugRenderTargets(iWidth, iHeight)))
			return E_FAIL;
#endif 
	}

	return S_OK;
}

#ifdef _DEBUG
HRESULT CGameInstance::Ready_RT_Debug(const _wstring& strTargetTag,	_float fX, _float fY, _float fSizeX, _float fSizeY,	_float fCanvasWidth, _float fCanvasHeight)
{
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	return m_pTarget_Manager->Ready_Debug(strTargetTag, fX, fY, fSizeX, fSizeY, fCanvasWidth, fCanvasHeight);
}
HRESULT CGameInstance::Render_RT_Debug(const _wstring& strMRTTag, CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
	if (nullptr == m_pTarget_Manager)
		return E_FAIL;

	return m_pTarget_Manager->Render_Debug(strMRTTag, pShader, pVIBuffer);
}
#endif
#pragma endregion

#pragma region COLLISION_MANAGER
void CGameInstance::Add_Collider(COLLISION_GROUP eGroup, CCollider* pCollider)
{
	m_pCollision_Manager->Add_Collider(eGroup, pCollider);
}

void CGameInstance::Set_CollisionMatrix(COLLISION_GROUP eA, COLLISION_GROUP eB, _bool bValue)
{
	m_pCollision_Manager->Set_CollisionMatrix(eA, eB, bValue);
}

void CGameInstance::Set_Debug_Colliders(_bool bValue)
{
	m_pCollision_Manager->Set_DebugDraw(bValue);
}

_bool CGameInstance::Is_Debug_Colliders() const
{
	return m_pCollision_Manager->Is_DebugDraw();
}
#pragma endregion

#pragma region PIPELINE
const _float4x4* CGameInstance::Get_Transform(D3DTS eState) const
{
	return m_pPipeLine->Get_Transform(eState);
}

const _float4x4* CGameInstance::Get_Transform_Inverse(D3DTS eState) const
{
	return m_pPipeLine->Get_Transform_Inverse(eState);
}

const _float4* CGameInstance::Get_CamPosition() const
{
	return m_pPipeLine->Get_CamPosition();
}

void CGameInstance::Compute_WorldRay(_float fViewportX, _float fViewportY, _float fViewportWidth, _float fViewportHeight, _float4* pRayOrigin, _float4* pRayDir)
{
	m_pPipeLine->Compute_WorldRay(fViewportX, fViewportY, fViewportWidth, fViewportHeight, pRayOrigin, pRayDir);
}

void			CGameInstance::Set_Transform(D3DTS eState, _fmatrix StateMatrix)
{
	m_pPipeLine->Set_Transform(eState, StateMatrix);
}

void CGameInstance::Transform_Frustum_ToLocalSpace(_fmatrix WorldMatrix)
{
	if (nullptr == m_pFrustum)
		return;

	m_pFrustum->Transform_ToLocalSpace(WorldMatrix);
}

_bool CGameInstance::Is_In_Frustum_WorldSpace(_fvector vWorldPos, _float fRange) const
{
	if (nullptr == m_pFrustum)
		return true;

	return m_pFrustum->Is_InWorldSpace(vWorldPos, fRange);
}

_bool CGameInstance::Is_In_Frustum_LocalSpace(_fvector vLocalPos, _float fRange) const
{
	if (nullptr == m_pFrustum)
		return true;

	return m_pFrustum->Is_InLocalSpace(vLocalPos, fRange);
}

const _float4x4* CGameInstance::Get_Shadow_Transform(D3DTS eState) const
{
	if (nullptr == m_pShadow)
		return nullptr;

	return m_pShadow->Get_Transform(eState);
}

HRESULT CGameInstance::Add_ShadowLight(const SHADOW_LIGHT_DESC& ShadowDesc)
{
	if (nullptr == m_pShadow)
		return E_FAIL;

	return m_pShadow->Add_ShadowLight(ShadowDesc);
}


#pragma endregion

#pragma region INPUT_DEVICE
_byte			CGameInstance::Get_KeyState(_ubyte byKeyID)
{
	return m_pInput_Device->Get_KeyState(byKeyID);
}

_bool			CGameInstance::Get_KeyDown(_ubyte byKeyID)
{
	return m_pInput_Device->Get_KeyDown(byKeyID);
}

_bool			CGameInstance::Get_KeyUp(_ubyte byKeyID)
{
	return m_pInput_Device->Get_KeyUp(byKeyID);
}

_byte			CGameInstance::Get_MouseBtnState(MOUSEBTN eBtn)
{
	return m_pInput_Device->Get_MouseBtnState(eBtn);
}

_bool			CGameInstance::Get_MouseBtnDown(MOUSEBTN eBtn)
{
	return m_pInput_Device->Get_MouseBtnDown(eBtn);
}

_bool			CGameInstance::Get_MouseBtnUp(MOUSEBTN eBtn)
{
	return m_pInput_Device->Get_MouseBtnUp(eBtn);
}

_long			CGameInstance::Get_MouseDelta(MOUSEAXIS eAxis)
{
	return m_pInput_Device->Get_MouseDelta(eAxis);
}

void		CGameInstance::Process_RawInput(LPARAM lParam)
{
	CInput_Device::Process_Input(lParam);
}

void		CGameInstance::Set_CursorLocked(_bool bLock)
{
	if (nullptr == m_pInput_Device)
		return;

	m_pInput_Device->Set_CursorLocked(bLock);
}

_bool		CGameInstance::Is_CursorLocked() const
{
	if (nullptr == m_pInput_Device)
		return false; 

	return m_pInput_Device->Is_CursorLocked();
}

#pragma endregion

#pragma region LIGHT_MANAGER

const LIGHT_DESC* CGameInstance::Get_LightDesc(_uint iIndex)
{
	return m_pLight_Manager->Get_LightDesc(iIndex);
}

HRESULT	CGameInstance::Add_Light(const LIGHT_DESC& LightDesc)
{
	return m_pLight_Manager->Add_Light(LightDesc);
}

HRESULT CGameInstance::Render_Light(CShader* pShader, CVIBuffer_Rect* pVIBuffer)
{
	return m_pLight_Manager->Render(pShader, pVIBuffer);
}

_uint CGameInstance::Get_NumLights() const
{
	if (nullptr == m_pLight_Manager)
		return 0;

	return m_pLight_Manager->Get_NumLights();
}

#pragma endregion

#pragma region SOUND_MANAGER

HRESULT CGameInstance::Play_Sound(const _wstring& strSoundKey, SOUND_CHANNEL eChannel, _float fVolume, _bool bLoop)
{
	if (nullptr == m_pSound_Manager)
		return E_FAIL;

	return m_pSound_Manager->Play_Sound(strSoundKey, eChannel, fVolume, bLoop);
}

HRESULT CGameInstance::Play_SoundSequence(const _wstring* pSoundKeys, _uint iNumSounds, SOUND_CHANNEL eChannel, _float fVolume)
{
	if (nullptr == m_pSound_Manager)
		return E_FAIL;

	return m_pSound_Manager->Play_SoundSequence(pSoundKeys, iNumSounds, eChannel, fVolume);
}

HRESULT CGameInstance::Play_BGM(const _wstring& strSoundKey, _float fVolume, _bool bLoop)
{
	if (nullptr == m_pSound_Manager)
		return E_FAIL;

	return m_pSound_Manager->Play_BGM(strSoundKey, fVolume, bLoop);
}

void CGameInstance::Stop_Sound(SOUND_CHANNEL eChannel)
{
	if (nullptr != m_pSound_Manager)
		m_pSound_Manager->Stop_Sound(eChannel);
}

void CGameInstance::Stop_AllSounds()
{
	if (nullptr != m_pSound_Manager)
		m_pSound_Manager->Stop_All();
}

void CGameInstance::Set_SoundVolume(SOUND_CHANNEL eChannel, _float fVolume)
{
	if (nullptr != m_pSound_Manager)
		m_pSound_Manager->Set_ChannelVolume(eChannel, fVolume);
}

_bool CGameInstance::Is_SoundPlaying(SOUND_CHANNEL eChannel) const
{
	if (nullptr == m_pSound_Manager)
		return false;

	return m_pSound_Manager->Is_Playing(eChannel);
}

#pragma endregion

#pragma region Font_MANAGER

void CGameInstance::Get_FontTags(vector<_wstring>* pOut)
{
	if (nullptr != m_pFont_Manager) 
		m_pFont_Manager->Get_FontTags(pOut);
}

HRESULT CGameInstance::Add_Font(const _wstring& strFontTag, const _tchar* pFontFilePath)
{
	return m_pFont_Manager->Add_Font(strFontTag, pFontFilePath);
}

HRESULT CGameInstance::Render_Font(const _wstring& strFontTag, const _tchar* pText, const _float2& vPosition,
	_fvector vColor, _float fRotation, const _float2& vOrigin, const _float2& vScale)
{
	return m_pFont_Manager->Draw(strFontTag, pText, vPosition, vColor, fRotation, vOrigin, vScale);
}

HRESULT CGameInstance::Measure_Font(const _wstring& strFontTag, const _tchar* pText, _float2* pOutSize)
{
	return m_pFont_Manager->Measure(strFontTag, pText, pOutSize);
}

#pragma endregion

void CGameInstance::Release_Engine()
{
	Safe_Release(m_pSound_Manager);
	Safe_Release(m_pShadow);
	Safe_Release(m_pCollision_Manager);
	Safe_Release(m_pFont_Manager);
	Safe_Release(m_pLight_Manager);
	Safe_Release(m_pInput_Device);
	Safe_Release(m_pFrustum);
	Safe_Release(m_pPipeLine);
	Safe_Release(m_pRenderer);
	Safe_Release(m_pTarget_Manager);
	Safe_Release(m_pObject_Manager);
	Safe_Release(m_pPrototype_Manager);
	Safe_Release(m_pLevel_Manager);
	Safe_Release(m_pTimer_Manager);
	Safe_Release(m_pGraphic_Device);

	DestroyInstance();
}

void CGameInstance::Free()
{
	__super::Free();
}