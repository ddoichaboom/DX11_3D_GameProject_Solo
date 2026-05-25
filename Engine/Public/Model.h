#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CModel final : public CComponent
{
private:
	CModel(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	CModel(const CModel& Prototype);
	virtual ~CModel() = default;

public:
#pragma region GET METHOD
	MODEL							Get_ModelType() const { return m_eModelType; }
	_uint							Get_NumMeshes() const { return m_iNumMeshes; }
	_uint							Get_NumMaterials() const { return m_iNumMaterials; }
	_uint							Get_NumBones() const { return m_iNumBones; }
	_uint							Get_NumAnimations() const { return m_iNumAnimations; }
	_float							Get_TrackPosition() const;
	_float							Get_Duration() const;
	
	_uint							Get_CurrentAnimIndex() const { return m_iCurrentAnimationIndex; }
	const _char*					Get_AnimationName(_uint iIndex) const;
	_bool							Get_AnimationPlaying() const { return m_isAnimPlaying; }
	_bool							Get_AnimationLoop() const { return m_isAnimLoop; }
	_bool							Get_AnimationLoop(_uint iIndex) const;
	_bool							Get_AnimationUseRootMotion(_uint iIndex) const;

	const _char*					Get_RootBoneName() const { return m_szRootBoneName; }
	_int							Get_RootBoneIndex() const { return m_iRootBoneIndex; }
	_bool							Get_RootMotionEnabled() const { return m_bRootMotionEnabled; }
	_float3							Get_LastRootMotionDelta() const { return m_vLastRootMotionDelta; }

	_int							Get_BoneIndex(const _char* pBoneName) const;
	const _float4x4*				Get_BoneCombinedMatrixPtr(_uint iBoneIndex) const;

	_int							Get_AnimationIndex(const _char* pAnimationName) const;

	const _float4x4&				Get_PreTransformMatrix() const { return m_PreTransformMatrix; }
	const _float4x4*				Get_BoneMatrixPtr(const _char* pBoneName) const;

	const _char*					Get_BoneName(_uint iIndex) const;
	_int							Get_BoneParentIndex(_uint iIndex) const;

	const _tchar*					Get_BinaryPath() const { return m_szBinaryPath; }

	_float							Get_TickPerSecond() const;

	_uint                           Get_NumNotifies(_uint iAnimIndex) const;
	ANIM_NOTIFY                     Get_Notify(_uint iAnimIndex, _uint iNotifyIndex) const;

	_bool							Get_LocalAABB(_float3& vOutCenter, _float3& vOutHalfExtent) const;
	_bool							Get_MeshLocalAABB(_uint iMeshIndex, _float3& vOutCenter, _float3& vOutHalfExtent) const;
	class CMesh*					Get_Mesh(_uint iMeshIndex) const;


#pragma endregion

#pragma region SET METHOD
	void							Set_ModelType(MODEL eType) { m_eModelType = eType; }
	void							Set_PreTransformMatrix(_fmatrix mat) { XMStoreFloat4x4(&m_PreTransformMatrix, mat); }
	void							Set_AnimationPlaying(_bool bPlay) { m_isAnimPlaying = bPlay; }

	void							Set_RootBoneName(const _char* pBoneName);
	void							Set_RootMotionEnabled(_bool bEnabled);
	void							Set_AnimationLoop(_uint iIndex, _bool bLoop);
	void							Set_AnimationUseRootMotion(_uint iIndex, _bool bUse);

	void                            Set_NotifyTick(_uint iAnimIndex, _uint iNotifyIndex, _float fTick);
	void                            Set_NotifyType(_uint iAnimIndex, _uint iNotifyIndex, ANIM_NOTIFY_TYPE eType);
	void                            Add_Notify(_uint iAnimIndex, const ANIM_NOTIFY& Notify);
	void                            Remove_Notify(_uint iAnimIndex, _uint iNotifyIndex);
	void                            Sort_Notifies(_uint iAnimIndex);
#pragma endregion

public:
	virtual HRESULT					Initialize_Prototype(const MODEL_DESC& Desc);
	virtual HRESULT					Initialize(void* pArg) override;

public:
	HRESULT							Bind_Material(class CShader* pShader, const _char* pConstantName,
												_uint iMeshIndex, TEXTURE_TYPE eType, _uint iTexIndex = 0);
	HRESULT							Bind_BoneMatrices(class CShader* pShader,
													const _char* pConstantName, _uint iMeshIndex);

	HRESULT							Render(_uint iMeshIndex);

	_bool							Pick(_fvector vRayOrigin, _fvector vRayDir, _fmatrix matWorld, _float& fDist);

	_bool							Play_Animation(_float fTimeDelta, class INotifyListener* pListener = nullptr);

	void							Set_AnimationIndex(_uint iIndex);
	void							Set_AnimationIndex_WithBlend(_uint iIndex, _float fBlendTime, _bool bRestartTo = true);
	HRESULT							Set_Animation(const _char* pAnimationName);
	void							Set_AnimationLoop(_bool bLoop);

	_bool                           Is_Blending() const { return m_bIsBlending; }
	_float                          Get_BlendWeight() const;

	HRESULT							Save_Binary() const;
	HRESULT							Save_Binary(const _tchar* pBinaryPath) const;

	void							Restart_Animation();


private:
	HRESULT							Ready_Meshes(const MODEL_DESC& Desc);
	HRESULT							Ready_Materials(const MODEL_DESC& Desc);
	HRESULT							Ready_Bones(const MODEL_DESC& Desc);
	HRESULT							Ready_Animations(const MODEL_DESC& Desc);

	void							Extract_RootMotion(_float fPrevTrackPos);
	void							Reset_RootMotionState();

	_bool							Play_Animation_Blended(_float fTimeDelta, class INotifyListener* pListener = nullptr);
	void							Extract_RootMotion_Blended(_float fWeight);


private:
	static HRESULT					Load_Binary_Desc(const _tchar* pBinaryPath, MODEL_DESC* pOutDesc);
	static void						Free_Binary_Desc(MODEL_DESC* pDesc);
	static HRESULT					Save_Binary_Desc(const _tchar* pBinaryPath, const MODEL_DESC& Desc);

private:
	MODEL							m_eModelType = { MODEL::END };

	_float4x4						m_PreTransformMatrix = {};

	_uint							m_iNumMeshes = {};
	vector<class CMesh*>			m_Meshes;

	_uint							m_iNumMaterials = {};
	vector<class CMaterial*>		m_Materials;

	_uint							m_iNumBones = {};
	vector<class CBone*>			m_Bones;

	_uint							m_iNumAnimations = {};
	vector<class CAnimation*>		m_Animations;
	unordered_map<string, _uint>	m_AnimationIndices;

	_uint							m_iCurrentAnimationIndex = {};
	_bool							m_isAnimLoop = { false };
	_bool							m_isAnimPlaying = { true };

	_bool							m_bIsBlending = { false };
	_bool							m_bFromIsStatic = { false };
	_uint							m_iBlendFromIndex = {};
	_uint                           m_iBlendToIndex = {};
	_float                          m_fBlendTime = { 0.f };
	_float                          m_fBlendDuration = { 0.f };

	vector<BONE_POSE>               m_PoseFrom;
	vector<BONE_POSE>               m_PoseTo;
	vector<BONE_POSE>               m_PoseOut;
	vector<_ubyte>					m_bHasPoseFrom;
	vector<_ubyte>					m_bHasPoseTo;

	_int							m_iRootBoneIndex = { -1 };

	_char							m_szRootBoneName[MAX_PATH] = {};
	_float3							m_vPrevRootTranslation = {};
	_float3							m_vBindRootTranslation = {};
	_float3							m_vLastRootMotionDelta = {};
	_bool							m_bRootMotionEnabled = { false };
	_bool							m_bRootMotionInitialized = { false };

	_float3                         m_vPrevRootTranslation_From = {};
	_float3                         m_vPrevRootTranslation_To = {};
	_bool                           m_bBlendRMInitialized_From = { false };
	_bool                           m_bBlendRMInitialized_To = { false };

	_tchar							m_szBinaryPath[MAX_PATH] = {};

	mutable _float3					m_vLocalAABBCenter = {};
	mutable _float3					m_vLocalAABBHalfExtent = {};
	mutable _bool					m_bLocalAABBCached = { false };

public:
	static CModel*					Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,const MODEL_DESC& Desc);
	static CModel*					Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,const _tchar* pBinaryPath);
	virtual CComponent*				Clone(void* pArg) override;
	virtual void					Free() override;
};

NS_END
