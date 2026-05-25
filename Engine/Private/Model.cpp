#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Bone.h"
#include "Animation.h"
#include "NotifyListener.h"

/* ============================================================================
   * Solo Leveling Model Data (.bin) 포맷 v1
   * ----------------------------------------------------------------------------
   * [Header]  (80 byte)
   *   char        magic[4]              = "SLMD"
   *   _uint       version               = 1
   *   _uint       modelType             (0=NONANIM, 1=ANIM)
   *   _uint       reserved              = 0
   *   _float4x4   PreTransformMatrix
   *
   * [Meshes]       _uint iNumMeshes + MESH_DESC[]
   *   char        szName[MAX_PATH]
   *   _uint       iMaterialIndex
   *   _uint       iVertexStride         (sizeof(VTXMESH) or VTXANIMMESH)
   *   _uint       iNumVertices
   *   byte        vertices[iNumVertices * iVertexStride]
   *   _uint       iNumIndices
   *   _uint       indices[iNumIndices]
   *   // ANIM 전용 본 바인딩:
   *   _uint       iNumBones (mesh-local)
   *   _uint       boneIndices[iNumBones]      (글로벌 본 인덱스)
   *   _float4x4   offsetMatrices[iNumBones]
   *
   * [Materials]    _uint iNumMaterials + MATERIAL_DESC[]
   *   _uint       iNumTextures
   *   for each tex: _uint eType + wchar_t szPath[MAX_PATH]   (Resources/ 루트 기준 상대)
   *
   * [Bones]        _uint iNumBones + BONE_DESC[]   (NONANIM 이면 0)
   *   char        szName[MAX_PATH]
   *   _int        iParentIndex
   *   _float4x4   TransformationMatrix
   *
   * [Animations]   _uint iNumAnimations + ANIMATION_DESC[] (NONANIM 이면 0)
   *   char        szName[MAX_PATH]
   *   _float      fDuration
   *   _float      fTickPerSecond
   *   _uint       bIsLoop (0/1)
   *   _uint       iNumChannels
   *   for each ch: _uint iBoneIndex + _uint iNumKeyFrames + KEYFRAME[iNumKeyFrames]
   * ============================================================================ */

namespace
{
	constexpr char SLMD_MAGIC[4]			= { 'S', 'L', 'M', 'D' };
	constexpr _uint SLMD_VERSION_LATEST		= 3;
	constexpr _uint SLMD_VERSION_MIN		= 1;
}

CModel::CModel(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CComponent { pDevice, pContext }
{
}

CModel::CModel(const CModel& Prototype)
	: CComponent{ Prototype }
	, m_eModelType { Prototype.m_eModelType }
	, m_PreTransformMatrix { Prototype.m_PreTransformMatrix }
	, m_iNumMeshes { Prototype.m_iNumMeshes }
	, m_Meshes {Prototype.m_Meshes }
	, m_iNumMaterials { Prototype.m_iNumMaterials }
	, m_Materials { Prototype.m_Materials }
	, m_iNumBones { Prototype.m_iNumBones }
	, m_iNumAnimations { Prototype.m_iNumAnimations }
	, m_AnimationIndices { Prototype.m_AnimationIndices }
	, m_iRootBoneIndex { Prototype.m_iRootBoneIndex }
	, m_bRootMotionEnabled { Prototype.m_bRootMotionEnabled }
{
	strcpy_s(m_szRootBoneName, Prototype.m_szRootBoneName);
	wcscpy_s(m_szBinaryPath, Prototype.m_szBinaryPath);

	for (auto& pMesh : m_Meshes)
		Safe_AddRef(pMesh);

	for (auto& pMaterial : m_Materials)
		Safe_AddRef(pMaterial);

	m_Bones.reserve(m_iNumBones);
	for (auto& pBone : Prototype.m_Bones)
		m_Bones.push_back(pBone->Clone());

	m_Animations.reserve(m_iNumAnimations);
	for (auto& pAnim : Prototype.m_Animations)
		m_Animations.push_back(pAnim->Clone());

	m_PoseFrom.resize(m_iNumBones);
	m_PoseTo.resize(m_iNumBones);
	m_PoseOut.resize(m_iNumBones);
	m_bHasPoseFrom.resize(m_iNumBones, 0);
	m_bHasPoseTo.resize(m_iNumBones, 0);
}

const _char* CModel::Get_AnimationName(_uint iIndex) const
{
	if (iIndex >= m_iNumAnimations)
		return "";

	return m_Animations[iIndex]->Get_Name();
}

_bool CModel::Get_AnimationLoop(_uint iIndex) const
{
	if (iIndex >= m_iNumAnimations)
		return false;

	return m_Animations[iIndex]->Get_IsLoop();
}

_bool CModel::Get_AnimationUseRootMotion(_uint iIndex) const
{
	if (iIndex >= m_iNumAnimations)
		return false;

	return m_Animations[iIndex]->Get_UseRootMotion();
}

_float CModel::Get_TrackPosition() const
{
	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return 0.f;

	return m_Animations[m_iCurrentAnimationIndex]->Get_CurrentTrackPosition();
}

_float CModel::Get_Duration() const
{
	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return 0.f;

	return m_Animations[m_iCurrentAnimationIndex]->Get_Duration();
}

_int CModel::Get_BoneIndex(const _char* pBoneName) const
{
	_int iIndex = { 0 };

	auto iter = find_if(m_Bones.begin(), m_Bones.end(), [&](CBone* pBone)
		{
			if (true == pBone->Compare_Name(pBoneName))
				return true;
			++iIndex;
			return false;
		});

	if (iter == m_Bones.end())
		return -1;

	return iIndex;
}

const _float4x4* CModel::Get_BoneCombinedMatrixPtr(_uint iBoneIndex) const
{
	if (iBoneIndex >= m_iNumBones)
		return nullptr;

	return m_Bones[iBoneIndex]->Get_CombinedTransformMatrixPtr();
}

_int CModel::Get_AnimationIndex(const _char* pAnimationName) const
{
	auto iter = m_AnimationIndices.find(pAnimationName);
	if (iter == m_AnimationIndices.end())
		return -1;

	return static_cast<_int>(iter->second);
}

const _float4x4* CModel::Get_BoneMatrixPtr(const _char* pBoneName) const
{
	_int iIndex = Get_BoneIndex(pBoneName);
	if (iIndex < 0)
		return nullptr;

	return m_Bones[iIndex]->Get_CombinedTransformMatrixPtr();
}

const _char* CModel::Get_BoneName(_uint iIndex) const
{
	if (iIndex >= m_iNumBones)
		return "";

	return m_Bones[iIndex]->Get_Name();
}

_int CModel::Get_BoneParentIndex(_uint iIndex) const
{
	if (iIndex >= m_iNumBones)
		return -1;

	return m_Bones[iIndex]->Get_ParentIndex();
}

void CModel::Set_RootBoneName(const _char* pBoneName)
{
	if (nullptr == pBoneName)
	{
		m_szRootBoneName[0] = '\0';
		m_iRootBoneIndex = -1;
		return;
	}

	strcpy_s(m_szRootBoneName, pBoneName);
	m_iRootBoneIndex = Get_BoneIndex(pBoneName);

	Reset_RootMotionState();
}

void CModel::Set_RootMotionEnabled(_bool bEnabled)
{
	m_bRootMotionEnabled = bEnabled;
	Reset_RootMotionState();
}

void CModel::Set_AnimationLoop(_uint iIndex, _bool bLoop)
{
	if (iIndex >= m_iNumAnimations)
		return;

	m_Animations[iIndex]->Set_IsLoop(bLoop);

	// 현재 재생 중인 애니라면 CModel 캐시(m_isAnimLoop)도 동기화
	if (iIndex == m_iCurrentAnimationIndex)
		m_isAnimLoop = bLoop;
}

void CModel::Set_AnimationUseRootMotion(_uint iIndex, _bool bUse)
{
	if (iIndex >= m_iNumAnimations)
		return;

	m_Animations[iIndex]->Set_UseRootMotion(bUse);

	if (iIndex == m_iCurrentAnimationIndex)
		Set_RootMotionEnabled(bUse);
}

HRESULT CModel::Initialize_Prototype(const MODEL_DESC& Desc)
{
	m_eModelType = Desc.eModelType;
	m_PreTransformMatrix = Desc.PreTransformMatrix;

	if (FAILED(Ready_Meshes(Desc)))
		return E_FAIL;

	if (FAILED(Ready_Materials(Desc)))
		return E_FAIL;

	if (FAILED(Ready_Bones(Desc)))
		return E_FAIL;

	if (FAILED(Ready_Animations(Desc)))
		return E_FAIL;

	if ('\0' != Desc.szRootBoneName[0])
		Set_RootBoneName(Desc.szRootBoneName);

	return S_OK;
}

HRESULT CModel::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT CModel::Bind_Material(CShader* pShader, const _char* pConstantName, _uint iMeshIndex, TEXTURE_TYPE eType, _uint iTexIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	_uint iMaterialIndex = m_Meshes[iMeshIndex]->Get_MaterialIndex();
	if (iMaterialIndex >= m_iNumMaterials)
		return E_FAIL;

	return m_Materials[iMaterialIndex]->Bind_ShaderResource(pShader, pConstantName, eType, iTexIndex);
}

HRESULT CModel::Bind_BoneMatrices(CShader* pShader, const _char* pConstantName, _uint iMeshIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	return m_Meshes[iMeshIndex]->Bind_BoneMatrices(pShader, pConstantName, m_Bones);
}

HRESULT CModel::Render(_uint iMeshIndex)
{
	if (iMeshIndex >= m_iNumMeshes)
		return E_FAIL;

	m_Meshes[iMeshIndex]->Bind_Resources();
	m_Meshes[iMeshIndex]->Render();

	return S_OK;
}

_bool CModel::Pick(_fvector vRayOrigin, _fvector vRayDir, _fmatrix matWorld, _float& fDist)
{
	if (MODEL::NONANIM == m_eModelType)
	{
		_float fMinDist = FLT_MAX;
		_bool bHit = { false };

		for (auto& pMesh : m_Meshes)
		{
			_float fMeshDist = { 0.f };
			if (pMesh->Pick(vRayOrigin, vRayDir, matWorld, fMeshDist))
			{
				if (fMeshDist < fMinDist)
				{
					fMinDist = fMeshDist;
					bHit = true;
				}
			}
		}

		if (bHit)
			fDist = fMinDist;

		return bHit;
	}

	// ANIM :CPU 스키닝 후 Picking
	_matrix matWorldInverse = XMMatrixInverse(nullptr, matWorld);
	_vector vLocalOrigin = XMVector3TransformCoord(vRayOrigin, matWorldInverse);
	_vector vLocalDir = XMVector3Normalize(XMVector3TransformNormal(vRayDir, matWorldInverse));

	_float fMinDist = FLT_MAX;
	_bool bHit = { false };

	for (auto& pMesh : m_Meshes)
	{
		PICK_DATA* pPickData = pMesh->Get_PickData();
		if (nullptr == pPickData)
			continue;

		const auto& BlendIndices = pMesh->Get_PickBlendIndices();
		const auto& BlendWeights = pMesh->Get_PickBlendWeights();

		// 블렌드 데이터 없으면 바인드 포즈 그대로 테스트
		if (BlendIndices.empty())
		{
			_float fMeshDist = { 0.f };
			if (pMesh->Pick(vRayOrigin, vRayDir, matWorld, fMeshDist))
			{
				if (fMeshDist < fMinDist)
				{
					fMinDist = fMeshDist;
					bHit = true;
				}
			}
			continue;
		}

		// (1) 본 행렬 계산 (Bind_BoneMatrices와 동일 로직)
		const auto& MeshBoneIndices = pMesh->Get_BoneIndices();
		const auto& OffsetMatrices = pMesh->Get_OffsetMatrices();
		_uint iNumMeshBones = pMesh->Get_NumBones();

		vector<_float4x4> BoneMatrices(iNumMeshBones);
		for (_uint b = 0; b < iNumMeshBones; ++b)
		{
			_matrix Offset = XMLoadFloat4x4(&OffsetMatrices[b]);
			_matrix Combined = XMLoadFloat4x4(
				m_Bones[MeshBoneIndices[b]]->Get_CombinedTransformMatrixPtr());
			XMStoreFloat4x4(&BoneMatrices[b], Offset * Combined);
		}

		// (2) CPU 스키닝 → 임시 정점 배열
		_uint iNumVerts = pPickData->iNumVertices;
		_float3* pSkinnedPos = new _float3[iNumVerts];

		for (_uint v = 0; v < iNumVerts; ++v)
		{
			XMUINT4  idx = BlendIndices[v];
			XMFLOAT4 wgt = BlendWeights[v];
			_vector  vPos = XMLoadFloat3(&pPickData->pVerticesPos[v]);

			_matrix matSkin =
				XMLoadFloat4x4(&BoneMatrices[idx.x]) * wgt.x +
				XMLoadFloat4x4(&BoneMatrices[idx.y]) * wgt.y +
				XMLoadFloat4x4(&BoneMatrices[idx.z]) * wgt.z +
				XMLoadFloat4x4(&BoneMatrices[idx.w]) * wgt.w;

			XMStoreFloat3(&pSkinnedPos[v],
				XMVector3TransformCoord(vPos, matSkin));
		}

		// (3) Ray vs 삼각형 테스트
		for (_uint i = 0; i < pPickData->iNumIndices; i += 3)
		{
			_vector v0 = XMLoadFloat3(&pSkinnedPos[pPickData->pIndices[i]]);
			_vector v1 = XMLoadFloat3(&pSkinnedPos[pPickData->pIndices[i + 1]]);
			_vector v2 = XMLoadFloat3(&pSkinnedPos[pPickData->pIndices[i + 2]]);

			_float fLocalDist = { 0.f };
			if (TriangleTests::Intersects(vLocalOrigin, vLocalDir,
				v0, v1, v2, fLocalDist))
			{
				if (fLocalDist < fMinDist)
				{
					fMinDist = fLocalDist;
					bHit = true;
				}
			}
		}

		delete[] pSkinnedPos;
	}

	if (bHit)
	{
		_vector vLocalHit = vLocalOrigin + vLocalDir * fMinDist;
		_vector vWorldHit = XMVector3TransformCoord(vLocalHit, matWorld);
		fDist = XMVectorGetX(XMVector3Length(vWorldHit - vRayOrigin));
	}

	return bHit;
}


_bool CModel::Play_Animation(_float fTimeDelta, INotifyListener* pListener)
{
	if (!m_isAnimPlaying)
		return false;

	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return false;

	if (true == m_bIsBlending)
		return Play_Animation_Blended(fTimeDelta, pListener);

	// 루프 감지용 : 업데이트 전 트랙 위치 캐시
	_float fPrevTrackPos = m_Animations[m_iCurrentAnimationIndex]->Get_CurrentTrackPosition();

	_bool isFinished = m_Animations[m_iCurrentAnimationIndex]->Update_TransformationMatrix(m_Bones, fTimeDelta, m_isAnimLoop, pListener);

	Extract_RootMotion(fPrevTrackPos);

	// Combined 행렬 갱신 (블렌딩/일반 공통)
	_matrix PreTransformMatrix = XMLoadFloat4x4(&m_PreTransformMatrix);

	for (auto& pBone : m_Bones)
		pBone->Update_CombinedTransformationMatrix(m_Bones, PreTransformMatrix);

	return isFinished;
}

void CModel::Set_AnimationIndex(_uint iIndex)
{
	if (iIndex >= m_iNumAnimations)
		return;

	if (iIndex == m_iCurrentAnimationIndex)
		return;
	
	m_bIsBlending = false;
	m_bFromIsStatic = false;

	if (m_iCurrentAnimationIndex < m_iNumAnimations)
		m_Animations[m_iCurrentAnimationIndex]->Reset_TrackPosition();


	m_iCurrentAnimationIndex = iIndex;
	m_Animations[m_iCurrentAnimationIndex]->Reset_TrackPosition();
	m_isAnimLoop = m_Animations[iIndex]->Get_IsLoop();

	Set_RootMotionEnabled(m_Animations[m_iCurrentAnimationIndex]->Get_UseRootMotion());	
}

void CModel::Set_AnimationIndex_WithBlend(_uint iIndex, _float fBlendTime, _bool bRestartTo)
{
	if (iIndex >= m_iNumAnimations)
		return;

	if (false == m_bIsBlending && iIndex == m_iCurrentAnimationIndex)
		return;
	if (true == m_bIsBlending && iIndex == m_iBlendToIndex)
		return;

	// BlendTime 0 이하 -> 즉시 전환
	if (fBlendTime <= 0.f)
	{
		m_bIsBlending = false;
		m_bFromIsStatic = false;
		Set_AnimationIndex(iIndex);
		return;
	}

	// 블렌딩 중 재요청 : 현재 보간된 Pose를 정적 From으로 캡처
	if (true == m_bIsBlending)
	{
		m_PoseFrom = m_PoseOut;

		for (size_t i = 0; i < m_iNumBones; i++)
			m_bHasPoseFrom[i] = (m_bHasPoseFrom[i] || m_bHasPoseTo[i]);

		m_bFromIsStatic = true;
	}
	// 정상 블렌딩 시작
	else
	{
		m_iBlendFromIndex = m_iCurrentAnimationIndex;
		m_bFromIsStatic = false;
	}

	m_iBlendToIndex = iIndex;

	if (true == bRestartTo)
		m_Animations[m_iBlendToIndex]->Reset_TrackPosition();

	m_fBlendTime = 0.f;
	m_fBlendDuration = fBlendTime;
	m_bIsBlending = true;

	m_bBlendRMInitialized_From = false;
	m_bBlendRMInitialized_To = false;

	if (m_iRootBoneIndex >= 0 && static_cast<_uint>(m_iRootBoneIndex) < m_iNumBones)
	{
		const _float4x4* pRootMat = m_Bones[m_iRootBoneIndex]->Get_TransformationMatrixPtr();
		m_vBindRootTranslation = { pRootMat->_41, pRootMat->_42, pRootMat->_43 };
	}

	// 블렌드 시작 지점에서 From / To의 Prev root Translation 미리 캡처 -> 첫 프레임 1틱 누락 방지
	const _uint iRoot = (m_iRootBoneIndex >= 0) ? static_cast<_uint>(m_iRootBoneIndex) : 0u;
	if (m_iRootBoneIndex >= 0 && iRoot < m_iNumBones)
	{
		if (false == m_bFromIsStatic && m_Animations[m_iBlendFromIndex]->Get_UseRootMotion())
		{
			BONE_POSE FromPose{};
			_ubyte hasFrom = {};

			for (auto& p : m_PoseFrom)
			{
				p.vScale = { 1.f, 1.f , 1.f };
				p.vRotation = { 0.f, 0.f, 0.f, 1.f };
				p.vTranslation = { 0.f, 0.f, 0.f };
			}
			fill(m_bHasPoseFrom.begin(), m_bHasPoseFrom.end(), 0);
			m_Animations[m_iBlendFromIndex]->Evaluate_Pose(m_PoseFrom.data(), m_bHasPoseFrom.data());
			if (0 != m_bHasPoseFrom[iRoot])
			{
				m_vPrevRootTranslation_From = m_PoseFrom[iRoot].vTranslation;
				m_bBlendRMInitialized_From = true;
			}
		}
		if (m_Animations[m_iBlendToIndex]->Get_UseRootMotion())
		{
			for (auto& p : m_PoseTo)
			{
				p.vScale = { 1.f, 1.f , 1.f };
				p.vRotation = { 0.f, 0.f, 0.f, 1.f };
				p.vTranslation = { 0.f, 0.f, 0.f };
			}
			fill(m_bHasPoseTo.begin(), m_bHasPoseTo.end(), 0);
			m_Animations[m_iBlendToIndex]->Evaluate_Pose(m_PoseTo.data(), m_bHasPoseTo.data());
			if (0 != m_bHasPoseTo[iRoot])
			{
				m_vPrevRootTranslation_To = m_PoseTo[iRoot].vTranslation;
				m_bBlendRMInitialized_To = true;
			}
		}
	}
}

HRESULT CModel::Set_Animation(const _char* pAnimationName)
{
	_int iIndex = Get_AnimationIndex(pAnimationName);
	if (-1 == iIndex)
		return E_FAIL;

	Set_AnimationIndex(static_cast<_uint>(iIndex));

	return S_OK;
}

void CModel::Set_AnimationLoop(_bool bLoop)
{
	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return;

	m_isAnimLoop = bLoop;
	m_Animations[m_iCurrentAnimationIndex]->Set_IsLoop(bLoop);
}

_float CModel::Get_BlendWeight() const
{
	if (false == m_bIsBlending || m_fBlendDuration <= 0.f) 
		return 1.f;

	_float w = m_fBlendTime / m_fBlendDuration;

	return (w < 0.f) ? 0.f : ((w > 1.f) ? 1.f : w);
}

_float CModel::Get_TickPerSecond() const
{
	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return 30.f;        // SLMD 기본치, div/0 방지
	return m_Animations[m_iCurrentAnimationIndex]->Get_TickPerSecond();
}

_uint CModel::Get_NumNotifies(_uint iAnimIndex) const
{
	if (iAnimIndex >= m_iNumAnimations)
		return 0;
	return m_Animations[iAnimIndex]->Get_NumNotifies();
}

ANIM_NOTIFY CModel::Get_Notify(_uint iAnimIndex, _uint iNotifyIndex) const
{
	ANIM_NOTIFY Empty{};        // {fTick=0, eType=NONE}
	if (iAnimIndex >= m_iNumAnimations)
		return Empty;
	if (iNotifyIndex >= m_Animations[iAnimIndex]->Get_NumNotifies())
		return Empty;
	return m_Animations[iAnimIndex]->Get_Notify(iNotifyIndex);
}

_bool CModel::Get_LocalAABB(_float3& vOutCenter, _float3& vOutHalfExtent) const
{
	if (true == m_bLocalAABBCached)
	{
		vOutCenter = m_vLocalAABBCenter;
		vOutHalfExtent = m_vLocalAABBHalfExtent;
		return true;
	}

	XMVECTOR vMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	XMVECTOR vMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
	_bool bAny = false;

	for (auto pMesh : m_Meshes)
	{
		if (nullptr == pMesh)
			continue;

		const PICK_DATA* pPick = pMesh->Get_PickData();
		if (nullptr == pPick || nullptr == pPick->pVerticesPos || 0 == pPick->iNumVertices)
			continue;

		for (_uint i = 0; i < pPick->iNumVertices; ++i)
		{
			XMVECTOR v = XMLoadFloat3(&pPick->pVerticesPos[i]);
			vMin = XMVectorMin(vMin, v);
			vMax = XMVectorMax(vMax, v);
			bAny = true;
		}
	}

	if (false == bAny)
		return false;

	XMVECTOR vCenter = XMVectorScale(XMVectorAdd(vMin, vMax), 0.5f);
	XMVECTOR vHalf = XMVectorScale(XMVectorSubtract(vMax, vMin), 0.5f);

	XMStoreFloat3(&m_vLocalAABBCenter, vCenter);
	XMStoreFloat3(&m_vLocalAABBHalfExtent, vHalf);
	m_bLocalAABBCached = true;

	vOutCenter = m_vLocalAABBCenter;
	vOutHalfExtent = m_vLocalAABBHalfExtent;
	return true;
}

_bool CModel::Get_MeshLocalAABB(_uint iMeshIndex, _float3& vOutCenter, _float3& vOutHalfExtent) const
{
	if (iMeshIndex >= m_iNumMeshes)
		return false;

	if (nullptr == m_Meshes[iMeshIndex])
		return false;

	return m_Meshes[iMeshIndex]->Get_LocalAABB(vOutCenter, vOutHalfExtent);
}

CMesh* CModel::Get_Mesh(_uint iMeshIndex) const
{
	if (iMeshIndex >= m_iNumMeshes)
		return nullptr;

	return m_Meshes[iMeshIndex];
}

void CModel::Set_NotifyTick(_uint iAnimIndex, _uint iNotifyIndex, _float fTick)
{
	if (iAnimIndex >= m_iNumAnimations)
		return;
	m_Animations[iAnimIndex]->Set_NotifyTick(iNotifyIndex, fTick);
}

void CModel::Set_NotifyType(_uint iAnimIndex, _uint iNotifyIndex, ANIM_NOTIFY_TYPE eType)
{
	if (iAnimIndex >= m_iNumAnimations)
		return;
	m_Animations[iAnimIndex]->Set_NotifyType(iNotifyIndex, eType);
}

void CModel::Add_Notify(_uint iAnimIndex, const ANIM_NOTIFY& Notify)
{
	if (iAnimIndex >= m_iNumAnimations)
		return;
	m_Animations[iAnimIndex]->Add_Notify(Notify);
}

void CModel::Remove_Notify(_uint iAnimIndex, _uint iNotifyIndex)
{
	if (iAnimIndex >= m_iNumAnimations)
		return;
	m_Animations[iAnimIndex]->Remove_Notify(iNotifyIndex);
}

void CModel::Sort_Notifies(_uint iAnimIndex)
{
	if (iAnimIndex >= m_iNumAnimations)
		return;
	m_Animations[iAnimIndex]->Sort_Notifies();
}

HRESULT CModel::Save_Binary() const
{
	if ('\0' == m_szBinaryPath[0])
		return E_FAIL;
	return Save_Binary(m_szBinaryPath);
}

HRESULT CModel::Save_Binary(const _tchar* pBinaryPath) const
{
	if (nullptr == pBinaryPath || '\0' == pBinaryPath[0])
		return E_FAIL;

	// 1) .bak 백업 (안전장치)
	_tchar szBackup[MAX_PATH] = {};
	swprintf_s(szBackup, L"%s.bak", pBinaryPath);
	CopyFileW(pBinaryPath, szBackup, FALSE);    // 실패해도 진행 (원본이 없는 신규 저장 케이스 등)

	// 2) 원본 .bin을 MODEL_DESC로 재로드 (메쉬/재질/본/채널 등 정적 데이터 확보)
	MODEL_DESC Desc{};
	if (FAILED(Load_Binary_Desc(pBinaryPath, &Desc)))
	{
		Free_Binary_Desc(&Desc);
		return E_FAIL;
	}

	// 3) 런타임 변경 사항을 Desc에 덮어쓰기
	strcpy_s(Desc.szRootBoneName, m_szRootBoneName);

	// 애니별 플래그 동기화 (Editor에서 토글한 값)
	_uint iAnimCount = (Desc.iNumAnimations < m_iNumAnimations) ? Desc.iNumAnimations : m_iNumAnimations;
	for (_uint i = 0; i < iAnimCount; ++i)
	{
		Desc.pAnimations[i].bIsLoop = m_Animations[i]->Get_IsLoop();
		Desc.pAnimations[i].bUseRootMotion = m_Animations[i]->Get_UseRootMotion();

		// C-5: 런타임 Notify 를 Desc 로 머지
		// Load_Binary_Desc 가 이미 채운 pNotifies 를 런타임 상태로 교체.
		// Safe_Delete_Array 는 nullptr-safe 이고 v2 폴백(빈 배열) 케이스도 안전 처리.
		Safe_Delete_Array(Desc.pAnimations[i].pNotifies);
		Desc.pAnimations[i].iNumNotifies = m_Animations[i]->Get_NumNotifies();

		if (Desc.pAnimations[i].iNumNotifies > 0)
		{
			Desc.pAnimations[i].pNotifies = new ANIM_NOTIFY[Desc.pAnimations[i].iNumNotifies]{};
			for (_uint k = 0; k < Desc.pAnimations[i].iNumNotifies; ++k)
				Desc.pAnimations[i].pNotifies[k] = m_Animations[i]->Get_Notify(k);
		}
	}

	// 4) v2 포맷으로 기록
	HRESULT hr = Save_Binary_Desc(pBinaryPath, Desc);

	// 5) 정리
	Free_Binary_Desc(&Desc);

	return hr;
}

void CModel::Restart_Animation()
{
	if (m_iCurrentAnimationIndex >= m_iNumAnimations)
		return;

	m_Animations[m_iCurrentAnimationIndex]->Reset_TrackPosition();
	Reset_RootMotionState();
}

HRESULT CModel::Ready_Meshes(const MODEL_DESC& Desc)
{
	m_iNumMeshes = Desc.iNumMeshes;
	m_Meshes.reserve(m_iNumMeshes);

	for (size_t i = 0; i < m_iNumMeshes; i++)
	{
		CMesh* pMesh = CMesh::Create(m_pDevice, m_pContext, Desc.eModelType, Desc.pMeshes[i]);
		if (nullptr == pMesh)
			return E_FAIL;

		m_Meshes.push_back(pMesh);
	}

	return S_OK;
}

HRESULT CModel::Ready_Materials(const MODEL_DESC& Desc)
{
	m_iNumMaterials = Desc.iNumMaterials;
	m_Materials.reserve(m_iNumMaterials);

	for (size_t i = 0; i < m_iNumMaterials; i++)
	{
		CMaterial* pMaterial = CMaterial::Create(m_pDevice, m_pContext);
		if (nullptr == pMaterial)
			return E_FAIL;

		const MATERIAL_DESC& MaterialDesc = Desc.pMaterials[i];

		for (size_t j = 0; j < MaterialDesc.iNumTextures; j++)
		{
			if (FAILED(pMaterial->Add_Texture(MaterialDesc.pTextures[j])))
			{
				Safe_Release(pMaterial);
				return E_FAIL;
			}
		}

		m_Materials.push_back(pMaterial);
	}

	return S_OK;
}

HRESULT CModel::Ready_Bones(const MODEL_DESC& Desc)
{
	m_iNumBones = Desc.iNumBones;
	m_Bones.reserve(m_iNumBones);

	for (size_t i = 0; i < m_iNumBones; i++)
	{
		CBone* pBone = CBone::Create(Desc.pBones[i]);
		if (nullptr == pBone)
			return E_FAIL;

		m_Bones.push_back(pBone);
	}

	m_PoseFrom.resize(m_iNumBones);
	m_PoseTo.resize(m_iNumBones);
	m_PoseOut.resize(m_iNumBones);
	m_bHasPoseFrom.resize(m_iNumBones, 0);
	m_bHasPoseTo.resize(m_iNumBones, 0);

	return S_OK;
}

HRESULT CModel::Ready_Animations(const MODEL_DESC& Desc)
{
	m_iNumAnimations = Desc.iNumAnimations;
	m_Animations.reserve(m_iNumAnimations);

	for (size_t i = 0; i < m_iNumAnimations; i++)
	{
		CAnimation* pAnimation = CAnimation::Create(Desc.pAnimations[i]);
		if (nullptr == pAnimation)
			return E_FAIL;

		m_Animations.push_back(pAnimation);

		// 이름 -> 벡터 인덱스 캐시 등록
		m_AnimationIndices.emplace(pAnimation->Get_Name(), i);
	}

	return S_OK;
}

void CModel::Extract_RootMotion(_float fPrevTrackPos)
{	
	// root 본 인덱스 검증 (모든 경로에서 사용)
	if (m_iRootBoneIndex < 0 || static_cast<_uint>(m_iRootBoneIndex) >= m_iNumBones)
	{
		m_vLastRootMotionDelta = {};
		return;
	}

	CBone* pRootBone = m_Bones[m_iRootBoneIndex];
	if (nullptr == pRootBone)
	{
		m_vLastRootMotionDelta = {};
		return;
	}

	const _float4x4* pMatrix = pRootBone->Get_TransformationMatrixPtr();
	_float3 T_cur = { pMatrix->_41, pMatrix->_42, pMatrix->_43 };

	_bool bClipUseRM = (m_iCurrentAnimationIndex < m_iNumAnimations) &&
		m_Animations[m_iCurrentAnimationIndex]->Get_UseRootMotion();

	if (m_bRootMotionEnabled && bClipUseRM)
	{
		_float fCurrTrackPos = m_Animations[m_iCurrentAnimationIndex]->Get_CurrentTrackPosition();
		_bool bWrapped = (fCurrTrackPos < fPrevTrackPos);

		if (!m_bRootMotionInitialized)
		{
			m_vBindRootTranslation = T_cur;
			m_vPrevRootTranslation = T_cur;
			m_vLastRootMotionDelta = {};
			m_bRootMotionInitialized = true;
		}
		else if (bWrapped)
		{
			m_vLastRootMotionDelta = {};
			m_vPrevRootTranslation = T_cur;
		}
		else
		{
			_float3 vLocalDelta{};
			vLocalDelta.x = T_cur.x - m_vPrevRootTranslation.x;
			vLocalDelta.y = T_cur.y - m_vPrevRootTranslation.y;
			vLocalDelta.z = T_cur.z - m_vPrevRootTranslation.z;

			m_vPrevRootTranslation = T_cur;

			_matrix matPre = XMLoadFloat4x4(&m_PreTransformMatrix);
			matPre.r[0] = XMVectorSetW(XMVector3Normalize(matPre.r[0]), 0.f);
			matPre.r[1] = XMVectorSetW(XMVector3Normalize(matPre.r[1]), 0.f);
			matPre.r[2] = XMVectorSetW(XMVector3Normalize(matPre.r[2]), 0.f);
			matPre.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);

			_vector vDelta = XMVectorSet(vLocalDelta.x, vLocalDelta.y, vLocalDelta.z, 0.f);
			vDelta = XMVector3TransformNormal(vDelta, matPre);
			XMStoreFloat3(&m_vLastRootMotionDelta, vDelta);
		}
	}
	else
	{
		m_vLastRootMotionDelta = {};
		m_bRootMotionInitialized = false;


		if (0.f == m_vBindRootTranslation.x &&
			0.f == m_vBindRootTranslation.y &&
			0.f == m_vBindRootTranslation.z)
		{
			m_vBindRootTranslation = T_cur;
		}
	}

	// 모든 경로에서 root 본을 bind 로 fix → 메쉬는 항상 같은 위치
	_float4x4 matFixed = *pMatrix;
	matFixed._41 = m_vBindRootTranslation.x;
	matFixed._42 = m_vBindRootTranslation.y;
	matFixed._43 = m_vBindRootTranslation.z;
	pRootBone->Set_TransformationMatrix(XMLoadFloat4x4(&matFixed));
}

void CModel::Reset_RootMotionState()
{
	m_bRootMotionInitialized = false;
	m_vPrevRootTranslation = {};
	m_vBindRootTranslation = {};
	m_vLastRootMotionDelta = {};
}

_bool CModel::Play_Animation_Blended(_float fTimeDelta, INotifyListener* pListener)
{
	// (1) Pose 작업 버퍼 reset
	auto ResetPose = [](BONE_POSE& p) {
		p.vScale = { 1.f, 1.f, 1.f };
		p.vRotation = { 0.f, 0.f, 0.f, 1.f };
		p.vTranslation = { 0.f, 0.f, 0.f };
	};

	// (2)  From 진행
	if (false == m_bFromIsStatic)
	{
		_bool bLoopFrom = m_Animations[m_iBlendFromIndex]->Get_IsLoop();
		m_Animations[m_iBlendFromIndex]->Advance_Time(fTimeDelta, bLoopFrom, pListener);

		fill(m_bHasPoseFrom.begin(), m_bHasPoseFrom.end(), 0);
		for (auto& p : m_PoseFrom) 
			ResetPose(p);
		m_Animations[m_iBlendFromIndex]->Evaluate_Pose(m_PoseFrom.data(), m_bHasPoseFrom.data());
	}

	// (3) To 진행
	{
		_bool bLoopTo = m_Animations[m_iBlendToIndex]->Get_IsLoop();
		m_Animations[m_iBlendToIndex]->Advance_Time(fTimeDelta, bLoopTo, pListener);

		fill(m_bHasPoseTo.begin(), m_bHasPoseTo.end(), 0);
		for (auto& p : m_PoseTo) 
			ResetPose(p);
		m_Animations[m_iBlendToIndex]->Evaluate_Pose(m_PoseTo.data(), m_bHasPoseTo.data());
	}

	// (4) Weight 갱신
	m_fBlendTime += fTimeDelta;
	_float w = (m_fBlendDuration > 0.f) ? (m_fBlendTime / m_fBlendDuration) : 1.f;
	if (w > 1.f)
		w = 1.f;

	// (5) Pose Lerp 
	for (size_t i = 0; i < m_iNumBones; i++)
	{
		_bool bF = (m_bHasPoseFrom[i] == 0) ? false : true;
		_bool bT = (m_bHasPoseTo[i] == 0) ? false : true;

		if (!bF && !bT)
			continue;

		const BONE_POSE& F = bF ? m_PoseFrom[i] : m_PoseTo[i];
		const BONE_POSE& T = bT ? m_PoseTo[i] : m_PoseFrom[i];

		_vector vS_F = XMLoadFloat3(&F.vScale);
		_vector vS_T = XMLoadFloat3(&T.vScale);
		_vector vR_F = XMLoadFloat4(&F.vRotation);
		_vector vR_T = XMLoadFloat4(&T.vRotation);
		_vector vT_F = XMLoadFloat3(&F.vTranslation);
		_vector vT_T = XMLoadFloat3(&T.vTranslation);

		_vector vS = XMVectorLerp(vS_F, vS_T, w);
		_vector vR = XMQuaternionSlerp(vR_F, vR_T, w);
		_vector vTr = XMVectorLerp(vT_F, vT_T, w);

		XMStoreFloat3(&m_PoseOut[i].vScale, vS);
		XMStoreFloat4(&m_PoseOut[i].vRotation, vR);
		XMStoreFloat3(&m_PoseOut[i].vTranslation, vTr);

		_matrix matLocal = XMMatrixAffineTransformation(vS, XMQuaternionIdentity(), vR, vTr);
		m_Bones[i]->Set_TransformationMatrix(matLocal);
	}

	// (6) RM Lerp 추출 + root 본 fix
	Extract_RootMotion_Blended(w);

	// (7) Combined Matrix 갱신
	_matrix PreTransformMatrix = XMLoadFloat4x4(&m_PreTransformMatrix);
	for (auto& pBone : m_Bones)
		pBone->Update_CombinedTransformationMatrix(m_Bones, PreTransformMatrix);

	// (8) 블렌드 완료 처리
	if (w >= 1.f)
	{
		//m_iCurrentAnimationIndex = m_iBlendToIndex;
		//m_isAnimLoop = m_Animations[m_iCurrentAnimationIndex]->Get_IsLoop();
		//Set_RootMotionEnabled(m_Animations[m_iCurrentAnimationIndex]->Get_UseRootMotion());

		//m_bIsBlending = false;
		//m_bFromIsStatic = false;
		//m_bBlendRMInitialized_From = false;
		//m_bBlendRMInitialized_To = false;

		m_iCurrentAnimationIndex = m_iBlendToIndex;
		m_isAnimLoop = m_Animations[m_iCurrentAnimationIndex]->Get_IsLoop();

		_bool bToUseRM = m_Animations[m_iCurrentAnimationIndex]->Get_UseRootMotion();

		_float3 vPrevTo_Backup = m_vPrevRootTranslation_To;
		_bool bInitTo_Backup = m_bBlendRMInitialized_To;
		_float3 vBind_Backup = m_vBindRootTranslation;

		Set_RootMotionEnabled(bToUseRM);

		if (bToUseRM && bInitTo_Backup)
      {
          m_vPrevRootTranslation = vPrevTo_Backup;
          m_vBindRootTranslation = vBind_Backup;
          m_bRootMotionInitialized = true;
      }

      m_bIsBlending = false;
      m_bFromIsStatic = false;
      m_bBlendRMInitialized_From = false;
      m_bBlendRMInitialized_To = false;
	}

	return false;
}

void CModel::Extract_RootMotion_Blended(_float fWeight)
{
	if (m_iRootBoneIndex < 0 || static_cast<_uint>(m_iRootBoneIndex) >= m_iNumBones)
	{
		m_vLastRootMotionDelta = {};
		return;
	}

	const _uint iRoot = static_cast<_uint>(m_iRootBoneIndex);

	_bool bUseFromRM = (false == m_bFromIsStatic)
					&& m_Animations[m_iBlendFromIndex]->Get_UseRootMotion();
	_bool bUseToRM = m_Animations[m_iBlendToIndex]->Get_UseRootMotion();

	// From Delta
	_float3 vDeltaFrom{};

	if (bUseFromRM && (0 != m_bHasPoseFrom[iRoot]))
	{
		const _float3 T_cur = m_PoseFrom[iRoot].vTranslation;
		CAnimation* pAnimFrom = m_Animations[m_iBlendFromIndex];
		_bool bWrap = (pAnimFrom->Get_CurrentTrackPosition() < pAnimFrom->Get_PrevTrackPosition());

		if (false == m_bBlendRMInitialized_From)
		{
			m_vPrevRootTranslation_From = T_cur;
			m_bBlendRMInitialized_From = true;
		}
		else if (bWrap)
		{
			m_vPrevRootTranslation_From = T_cur;
		}
		else
		{
			vDeltaFrom.x = T_cur.x - m_vPrevRootTranslation_From.x;
			vDeltaFrom.y = T_cur.y - m_vPrevRootTranslation_From.y;
			vDeltaFrom.z = T_cur.z - m_vPrevRootTranslation_From.z;
			m_vPrevRootTranslation_From = T_cur;
		}
	}

	// To - Delta
	_float3 vDeltaTo{};
	if (bUseToRM && (0 != m_bHasPoseTo[iRoot]))
	{
		const _float3 T_cur = m_PoseTo[iRoot].vTranslation;
		CAnimation* pAnimTo = m_Animations[m_iBlendToIndex];
		_bool bWrap = (pAnimTo->Get_CurrentTrackPosition() < pAnimTo->Get_PrevTrackPosition());

		if (false == m_bBlendRMInitialized_To)
		{
			m_vPrevRootTranslation_To = T_cur;
			m_bBlendRMInitialized_To = true;
		}
		else if (bWrap)
		{
			m_vPrevRootTranslation_To = T_cur;
		}
		else
		{
			vDeltaTo.x = T_cur.x - m_vPrevRootTranslation_To.x;
			vDeltaTo.y = T_cur.y - m_vPrevRootTranslation_To.y;
			vDeltaTo.z = T_cur.z - m_vPrevRootTranslation_To.z;
			m_vPrevRootTranslation_To = T_cur;
		}
	}

	// 두 Delta Lerp
	_float3 vLerped{};
	vLerped.x = vDeltaFrom.x * (1.f - fWeight) + vDeltaTo.x * fWeight;
	vLerped.y = vDeltaFrom.y * (1.f - fWeight) + vDeltaTo.y * fWeight;
	vLerped.z = vDeltaFrom.z * (1.f - fWeight) + vDeltaTo.z * fWeight;

	// PreTransform 회전 적용
	_matrix matPre = XMLoadFloat4x4(&m_PreTransformMatrix);
	matPre.r[0] = XMVectorSetW(XMVector3Normalize(matPre.r[0]), 0.f);
	matPre.r[1] = XMVectorSetW(XMVector3Normalize(matPre.r[1]), 0.f);
	matPre.r[2] = XMVectorSetW(XMVector3Normalize(matPre.r[2]), 0.f);
	matPre.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);

	_vector vDelta = XMVectorSet(vLerped.x, vLerped.y, vLerped.z, 0.f);
	vDelta = XMVector3TransformNormal(vDelta, matPre);
	XMStoreFloat3(&m_vLastRootMotionDelta, vDelta);

	// root 본을 bind 위치로 fix 
	CBone* pRootBone = m_Bones[iRoot];
	const _float4x4* pMatrix = pRootBone->Get_TransformationMatrixPtr();
	_float4x4 matFixed = *pMatrix;
	matFixed._41 = m_vBindRootTranslation.x;
	matFixed._42 = m_vBindRootTranslation.y;
	matFixed._43 = m_vBindRootTranslation.z;
	pRootBone->Set_TransformationMatrix(XMLoadFloat4x4(&matFixed));
}

HRESULT CModel::Load_Binary_Desc(const _tchar* pBinaryPath, MODEL_DESC* pOutDesc)
{
	FILE* fp = nullptr;
	if (0 != _wfopen_s(&fp, pBinaryPath, L"rb") || nullptr == fp)
		return E_FAIL;

	/* ===== Header ===== */
	char magic[4] = {};
	fread(magic, sizeof(char), 4, fp);
	if (0 != memcmp(magic, SLMD_MAGIC, 4))
	{
		fclose(fp);
		return E_FAIL;
	}

	_uint iVersion = 0;
	fread(&iVersion, sizeof(_uint), 1, fp);
	if (iVersion < SLMD_VERSION_MIN || iVersion > SLMD_VERSION_LATEST)
	{
		fclose(fp);
		return E_FAIL;
	}

	_uint iModelType = 0;
	fread(&iModelType, sizeof(_uint), 1, fp);
	pOutDesc->eModelType = static_cast<MODEL>(iModelType);

	_uint iReserved = 0;
	fread(&iReserved, sizeof(_uint), 1, fp);

	fread(&pOutDesc->PreTransformMatrix, sizeof(_float4x4), 1, fp);

	if (iVersion >= 2)
	{
		fread(pOutDesc->szRootBoneName, sizeof(char), MAX_PATH, fp);
	}

	/* ===== Meshes ===== */
	fread(&pOutDesc->iNumMeshes, sizeof(_uint), 1, fp);
	pOutDesc->pMeshes = new MESH_DESC[pOutDesc->iNumMeshes]{};

	for (_uint i = 0; i < pOutDesc->iNumMeshes; ++i)
	{
		MESH_DESC& Mesh = pOutDesc->pMeshes[i];

		fread(Mesh.szName, sizeof(char), MAX_PATH, fp);
		fread(&Mesh.iMaterialIndex, sizeof(_uint), 1, fp);
		fread(&Mesh.iVertexStride, sizeof(_uint), 1, fp);
		fread(&Mesh.iNumVertices, sizeof(_uint), 1, fp);

		// 정점 스트라이드 기반 raw byte 할당 (VTXMESH/VTXANIMMESH 구분 없음)
		Mesh.pVertices = new _ubyte[static_cast<size_t>(Mesh.iVertexStride) * Mesh.iNumVertices];
		fread(Mesh.pVertices, Mesh.iVertexStride, Mesh.iNumVertices, fp);

		fread(&Mesh.iNumIndices, sizeof(_uint), 1, fp);
		Mesh.pIndices = new _uint[Mesh.iNumIndices]{};
		fread(Mesh.pIndices, sizeof(_uint), Mesh.iNumIndices, fp);

		// Anim 전용 본 바인딩
		if (MODEL::ANIM == pOutDesc->eModelType)
		{
			fread(&Mesh.iNumBones, sizeof(_uint), 1, fp);
			if (Mesh.iNumBones > 0)
			{
				Mesh.pBoneIndices = new _uint[Mesh.iNumBones]{};
				fread(Mesh.pBoneIndices, sizeof(_uint), Mesh.iNumBones, fp);

				Mesh.pOffsetMatrices = new _float4x4[Mesh.iNumBones]{};
				fread(Mesh.pOffsetMatrices, sizeof(_float4x4), Mesh.iNumBones, fp);
			}
		}
	}

	/* ===== Materials ===== */
	fread(&pOutDesc->iNumMaterials, sizeof(_uint), 1, fp);
	pOutDesc->pMaterials = new MATERIAL_DESC[pOutDesc->iNumMaterials]{};

	for (_uint i = 0; i < pOutDesc->iNumMaterials; ++i)
	{
		MATERIAL_DESC& Mat = pOutDesc->pMaterials[i];

		fread(&Mat.iNumTextures, sizeof(_uint), 1, fp);
		Mat.pTextures = new MATERIAL_TEXTURE_DESC[Mat.iNumTextures]{};

		for (_uint j = 0; j < Mat.iNumTextures; ++j)
		{
			_uint iTexType = 0;
			fread(&iTexType, sizeof(_uint), 1, fp);
			Mat.pTextures[j].eType = static_cast<TEXTURE_TYPE>(iTexType);
			fread(Mat.pTextures[j].szPath, sizeof(wchar_t), MAX_PATH, fp);
		}
	}

	/* ===== Bones ===== */
	fread(&pOutDesc->iNumBones, sizeof(_uint), 1, fp);
	if (pOutDesc->iNumBones > 0)
	{
		pOutDesc->pBones = new BONE_DESC[pOutDesc->iNumBones]{};
		for (_uint i = 0; i < pOutDesc->iNumBones; ++i)
		{
			BONE_DESC& Bone = pOutDesc->pBones[i];
			fread(Bone.szName, sizeof(char), MAX_PATH, fp);
			fread(&Bone.iParentIndex, sizeof(_int), 1, fp);
			fread(&Bone.TransformationMatrix, sizeof(_float4x4), 1, fp);
		}
	}

	/* ===== Animations ===== */
	fread(&pOutDesc->iNumAnimations, sizeof(_uint), 1, fp);
	if (pOutDesc->iNumAnimations > 0)
	{
		pOutDesc->pAnimations = new ANIMATION_DESC[pOutDesc->iNumAnimations]{};
		for (_uint i = 0; i < pOutDesc->iNumAnimations; ++i)
		{
			ANIMATION_DESC& Anim = pOutDesc->pAnimations[i];

			fread(Anim.szName, sizeof(char), MAX_PATH, fp);
			fread(&Anim.fDuration, sizeof(_float), 1, fp);
			fread(&Anim.fTickPerSecond, sizeof(_float), 1, fp);

			_uint iIsLoop = 0;
			fread(&iIsLoop, sizeof(_uint), 1, fp);
			Anim.bIsLoop = (0 != iIsLoop);

			if (iVersion >= 2)
			{
				_uint iUseRm = 0;
				fread(&iUseRm, sizeof(_uint), 1, fp);
				Anim.bUseRootMotion = (0 != iUseRm);
			}

			fread(&Anim.iNumChannels, sizeof(_uint), 1, fp);
			Anim.pChannels = new CHANNEL_DESC[Anim.iNumChannels]{};

			for (_uint j = 0; j < Anim.iNumChannels; ++j)
			{
				CHANNEL_DESC& Ch = Anim.pChannels[j];
				fread(&Ch.iBoneIndex, sizeof(_uint), 1, fp);
				fread(&Ch.iNumKeyFrames, sizeof(_uint), 1, fp);
				Ch.pKeyFrames = new KEYFRAME[Ch.iNumKeyFrames]{};
				fread(Ch.pKeyFrames, sizeof(KEYFRAME), Ch.iNumKeyFrames, fp);
			}

			if (iVersion >= 3)
			{
				fread(&Anim.iNumNotifies, sizeof(_uint), 1, fp);
				if (Anim.iNumNotifies > 0)
				{
					Anim.pNotifies = new ANIM_NOTIFY[Anim.iNumNotifies]{};
					for (_uint k = 0; k < Anim.iNumNotifies; ++k)
					{
						fread(&Anim.pNotifies[k].fTick, sizeof(_float), 1, fp);

						_uint iType = 0;
						fread(&iType, sizeof(_uint), 1, fp);
						Anim.pNotifies[k].eType = static_cast<ANIM_NOTIFY_TYPE>(iType);
					}
				}
			}
		}
	}

	fclose(fp);
	return S_OK;
}

void CModel::Free_Binary_Desc(MODEL_DESC* pDesc)
{
	// Animations
	if (nullptr != pDesc->pAnimations)
	{
		for (_uint i = 0; i < pDesc->iNumAnimations; ++i)
		{
			ANIMATION_DESC& Anim = pDesc->pAnimations[i];
			if (nullptr != Anim.pChannels)
			{
				for (_uint j = 0; j < Anim.iNumChannels; ++j)
					Safe_Delete_Array(Anim.pChannels[j].pKeyFrames);

				Safe_Delete_Array(Anim.pChannels);
			}
			Safe_Delete_Array(Anim.pNotifies);
		}
		Safe_Delete_Array(pDesc->pAnimations);
	}

	// Bones
	Safe_Delete_Array(pDesc->pBones);

	// Materials
	if (nullptr != pDesc->pMaterials)
	{
		for (_uint i = 0; i < pDesc->iNumMaterials; ++i)
			Safe_Delete_Array(pDesc->pMaterials[i].pTextures);

		Safe_Delete_Array(pDesc->pMaterials);
	}

	// Meshes
	if (nullptr != pDesc->pMeshes)
	{
		for (_uint i = 0; i < pDesc->iNumMeshes; ++i)
		{
			MESH_DESC& Mesh = pDesc->pMeshes[i];

			// pVertices 는 _ubyte 배열로 할당했으므로 _ubyte* 로 캐스팅 후 delete[]
			if (nullptr != Mesh.pVertices)
			{
				delete[] static_cast<_ubyte*>(Mesh.pVertices);
				Mesh.pVertices = nullptr;
			}

			Safe_Delete_Array(Mesh.pIndices);
			Safe_Delete_Array(Mesh.pBoneIndices);
			Safe_Delete_Array(Mesh.pOffsetMatrices);
		}
		Safe_Delete_Array(pDesc->pMeshes);
	}
}

HRESULT CModel::Save_Binary_Desc(const _tchar* pBinaryPath, const MODEL_DESC& Desc)
{
	FILE* fp = nullptr;
	if (0 != _wfopen_s(&fp, pBinaryPath, L"wb") || nullptr == fp)
		return E_FAIL;

	/* ===== Header (v2) ===== */
	fwrite(SLMD_MAGIC, sizeof(char), 4, fp);

	_uint iVersion = SLMD_VERSION_LATEST;       // 2
	fwrite(&iVersion, sizeof(_uint), 1, fp);

	_uint iModelType = static_cast<_uint>(Desc.eModelType);
	fwrite(&iModelType, sizeof(_uint), 1, fp);

	_uint iReserved = 0;
	fwrite(&iReserved, sizeof(_uint), 1, fp);

	fwrite(&Desc.PreTransformMatrix, sizeof(_float4x4), 1, fp);

	// v2: Root Bone Name
	fwrite(Desc.szRootBoneName, sizeof(char), MAX_PATH, fp);

	/* ===== Meshes ===== */
	fwrite(&Desc.iNumMeshes, sizeof(_uint), 1, fp);
	for (_uint i = 0; i < Desc.iNumMeshes; ++i)
	{
		const MESH_DESC& Mesh = Desc.pMeshes[i];

		fwrite(Mesh.szName, sizeof(char), MAX_PATH, fp);
		fwrite(&Mesh.iMaterialIndex, sizeof(_uint), 1, fp);
		fwrite(&Mesh.iVertexStride, sizeof(_uint), 1, fp);
		fwrite(&Mesh.iNumVertices, sizeof(_uint), 1, fp);
		fwrite(Mesh.pVertices, Mesh.iVertexStride, Mesh.iNumVertices, fp);

		fwrite(&Mesh.iNumIndices, sizeof(_uint), 1, fp);
		fwrite(Mesh.pIndices, sizeof(_uint), Mesh.iNumIndices, fp);

		if (MODEL::ANIM == Desc.eModelType)
		{
			fwrite(&Mesh.iNumBones, sizeof(_uint), 1, fp);
			if (Mesh.iNumBones > 0)
			{
				fwrite(Mesh.pBoneIndices, sizeof(_uint), Mesh.iNumBones, fp);
				fwrite(Mesh.pOffsetMatrices, sizeof(_float4x4), Mesh.iNumBones, fp);
			}
		}
	}

	/* ===== Materials ===== */
	fwrite(&Desc.iNumMaterials, sizeof(_uint), 1, fp);
	for (_uint i = 0; i < Desc.iNumMaterials; ++i)
	{
		const MATERIAL_DESC& Mat = Desc.pMaterials[i];
		fwrite(&Mat.iNumTextures, sizeof(_uint), 1, fp);
		for (_uint j = 0; j < Mat.iNumTextures; ++j)
		{
			_uint iTexType = static_cast<_uint>(Mat.pTextures[j].eType);
			fwrite(&iTexType, sizeof(_uint), 1, fp);
			fwrite(Mat.pTextures[j].szPath, sizeof(wchar_t), MAX_PATH, fp);
		}
	}

	/* ===== Bones ===== */
	fwrite(&Desc.iNumBones, sizeof(_uint), 1, fp);
	for (_uint i = 0; i < Desc.iNumBones; ++i)
	{
		const BONE_DESC& Bone = Desc.pBones[i];
		fwrite(Bone.szName, sizeof(char), MAX_PATH, fp);
		fwrite(&Bone.iParentIndex, sizeof(_int), 1, fp);
		fwrite(&Bone.TransformationMatrix, sizeof(_float4x4), 1, fp);
	}

	/* ===== Animations ===== */
	fwrite(&Desc.iNumAnimations, sizeof(_uint), 1, fp);
	for (_uint i = 0; i < Desc.iNumAnimations; ++i)
	{
		const ANIMATION_DESC& Anim = Desc.pAnimations[i];

		fwrite(Anim.szName, sizeof(char), MAX_PATH, fp);
		fwrite(&Anim.fDuration, sizeof(_float), 1, fp);
		fwrite(&Anim.fTickPerSecond, sizeof(_float), 1, fp);

		_uint iIsLoop = Anim.bIsLoop ? 1u : 0u;
		fwrite(&iIsLoop, sizeof(_uint), 1, fp);

		// v2: bUseRootMotion
		_uint iUseRM = Anim.bUseRootMotion ? 1u : 0u;
		fwrite(&iUseRM, sizeof(_uint), 1, fp);

		fwrite(&Anim.iNumChannels, sizeof(_uint), 1, fp);
		for (_uint j = 0; j < Anim.iNumChannels; ++j)
		{
			const CHANNEL_DESC& Ch = Anim.pChannels[j];
			fwrite(&Ch.iBoneIndex, sizeof(_uint), 1, fp);
			fwrite(&Ch.iNumKeyFrames, sizeof(_uint), 1, fp);
			fwrite(Ch.pKeyFrames, sizeof(KEYFRAME), Ch.iNumKeyFrames, fp);
		}

		fwrite(&Anim.iNumNotifies, sizeof(_uint), 1, fp);
		for (_uint k = 0; k < Anim.iNumNotifies; ++k)
		{
			fwrite(&Anim.pNotifies[k].fTick, sizeof(_float), 1, fp);
			_uint iType = static_cast<_uint>(Anim.pNotifies[k].eType);
			fwrite(&iType, sizeof(_uint), 1, fp);
		}
	}

	fclose(fp);
	return S_OK;
}

CModel* CModel::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const MODEL_DESC& Desc)
{
	CModel* pInstance = new CModel(pDevice, pContext);

	if (FAILED(pInstance->Initialize_Prototype(Desc)))
	{
		MSG_BOX("Failed to Created : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

CModel* CModel::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const _tchar* pBinaryPath)
{
	MODEL_DESC Desc{};

	if (FAILED(Load_Binary_Desc(pBinaryPath, &Desc)))
	{
		Free_Binary_Desc(&Desc);    // 부분 할당된 상태 해제
		MSG_BOX("Failed to Load Binary : CModel");
		return nullptr;
	}

	CModel* pInstance = new CModel(pDevice, pContext);

	wcscpy_s(pInstance->m_szBinaryPath, pBinaryPath);

	HRESULT hr = pInstance->Initialize_Prototype(Desc);

	// Initialize_Prototype 가 내부 구조체로 복사 완료했으므로 임시 Desc 해제
	Free_Binary_Desc(&Desc);

	if (FAILED(hr))
	{
		MSG_BOX("Failed to Created : CModel (Binary)");
		Safe_Release(pInstance);
		return nullptr;
	}

	return pInstance;
}

CComponent* CModel::Clone(void* pArg)
{
	CModel* pInstance = new CModel(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : CModel");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CModel::Free()
{
	__super::Free();

	for (auto& pAnim : m_Animations)
		Safe_Release(pAnim);
	m_Animations.clear();
	m_AnimationIndices.clear();

	for (auto& pBone : m_Bones)
		Safe_Release(pBone);
	m_Bones.clear();

	for (auto& pMaterial : m_Materials)
		Safe_Release(pMaterial);
	m_Materials.clear();

	for (auto& pMesh : m_Meshes)
		Safe_Release(pMesh);
	m_Meshes.clear();
}
