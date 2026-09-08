#ifndef Engine_Struct_h__
#define Engine_Struct_h__

namespace Engine
{
	typedef struct tagEngineDesc
	{
		HINSTANCE		hInstance;
		HWND			hWnd;
		WINMODE			eWinMode;
		unsigned int	iViewportWidth, iViewportHeight;
		unsigned int	iNumLevels;
	}ENGINE_DESC;

	typedef struct tagLightDesc
	{
		LIGHT		eType;									// 광원 타입 (예: 방향성 광원, 점 광원 등)
		XMFLOAT4	vDiffuse, vAmbient, vSpecular;			// 빛의 3가지 색상 성분
		XMFLOAT4	vDirection;								// 방향광용 : 빛이 향하는 방향
		XMFLOAT4	vPosition;								// 점광원용 : 광원 위치
		float		fRange;									// 점광원용 : 광원의 유효 범위
	}LIGHT_DESC;

	typedef struct tagShadowLightDesc
	{
		XMFLOAT4	vEye;
		XMFLOAT4	vAt;
		float		fFovy;
		float		fAspect;
		float		fNear;
		float		fFar;

		bool		bOrthographic = { false };
		float		fOrthoWidth = { 100.f };
		float		fOrthoHeight = { 100.f };
	}SHADOW_LIGHT_DESC;

	typedef struct tagVertexPosition
	{
		XMFLOAT3 vPosition;

		static const unsigned int iNumElements = { 1 };

		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

	}VTXPOS;

	typedef struct tagVertexPositionTexcoord
	{
		XMFLOAT3		vPosition;		
		XMFLOAT2		vTexcoord;		

		static const unsigned int iNumElements = { 2 };

		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

	}VTXTEX;
	typedef struct tagVertexRectInstance
	{
		XMFLOAT4		vRight;
		XMFLOAT4		vUp;
		XMFLOAT4		vLook;
		XMFLOAT4		vTranslation;
		XMFLOAT4		vTexInfo;
		XMFLOAT4		vColor;

		static const unsigned int iNumElements = { 8 };

		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{"TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
			{"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1}
		};

	}VTXRECT_INSTANCE;

	typedef struct tagVertexPositionNormalTexcoord
	{
		XMFLOAT3		vPosition;
		XMFLOAT3		vNormal;
		XMFLOAT2		vTexcoord;

		static const unsigned int		iNumElements = { 3 };

		static constexpr D3D11_INPUT_ELEMENT_DESC   Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

	}VTXNORTEX;

	typedef struct tagVertexMesh
	{
		XMFLOAT3		vPosition;
		XMFLOAT3		vNormal;
		XMFLOAT2		vTexcoord;
		XMFLOAT3		vTangent;
		XMFLOAT3		vBinormal;

		static const unsigned int		iNumElements = { 5 };

		static constexpr D3D11_INPUT_ELEMENT_DESC   Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
	}VTXMESH;

	typedef struct tagVertexAnimationMesh
	{
		XMFLOAT3		vPosition;
		XMFLOAT3		vNormal;
		XMFLOAT2		vTexcoord;
		XMFLOAT3		vTangent;
		XMFLOAT3		vBinormal;
		XMUINT4			vBlendIndex;		// 영향 주는 Bone 4개의 인덱스
		XMFLOAT4		vBlendWeight;		// Bone 4개의 가중치 (합 = 1.0)

		static const unsigned int		iNumElements = { 7 };

		static constexpr D3D11_INPUT_ELEMENT_DESC   Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 72, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};


	}VTXANIMMESH;

	typedef struct tagPickingData
	{
		XMFLOAT3*			pVerticesPos = { nullptr };
		unsigned int*		pIndices = { nullptr };
		unsigned int		iNumVertices = {};
		unsigned int 		iNumIndices = {};
	}PICK_DATA;

	typedef struct tagKeyFrame
	{
		XMFLOAT3	vScale;
		XMFLOAT4	vRotation;			// 쿼터니언
		XMFLOAT3	vTranslation;
		float		fTrackPosition;
	}KEYFRAME;

	typedef struct tagAnimNotify
	{
		float				fTick = {};
		ANIM_NOTIFY_TYPE	eType = { ANIM_NOTIFY_TYPE::NONE };
	}ANIM_NOTIFY;

	typedef struct tagBonePose
	{
		XMFLOAT3	vScale = { 1.f, 1.f, 1.f };
		XMFLOAT4	vRotation = { 0.f, 0.f, 0.f, 1.f };
		XMFLOAT3	vTranslation = { 0.f, 0.f, 0.f };
	}BONE_POSE;

	typedef struct tagMeshDesc
	{
		char			szName[MAX_PATH] = {};
		unsigned int	iMaterialIndex = {};

		// 정점/인덱스 (NonAnim & Anim 공용)
		void*			pVertices = { nullptr };						// VTXMESH* 또는 VTXANIMMESH*
		unsigned int	iNumVertices = {};
		unsigned int	iVertexStride = {};

		unsigned int*	pIndices = { nullptr };
		unsigned int	iNumIndices = {};

		// Anim 전용
		unsigned int	iNumBones = {};
		unsigned int*	pBoneIndices = { nullptr };			// 글로벌 본 인덱스 배열
		XMFLOAT4X4*		pOffsetMatrices = { nullptr };
	}MESH_DESC;

	typedef struct tagChannelDesc
	{
		unsigned int	iBoneIndex = {};
		unsigned int	iNumKeyFrames = {};
		KEYFRAME*		pKeyFrames = { nullptr };		
	}CHANNEL_DESC;

	typedef struct tagAnimationDesc
	{
		char			szName[MAX_PATH] = {};
		float			fDuration = {};
		float			fTickPerSecond = {};
		bool			bIsLoop	= { false };
		bool			bUseRootMotion = { false };
		unsigned int	iNumChannels = {};
		CHANNEL_DESC*	pChannels = { nullptr };

		unsigned int	iNumNotifies = {};
		ANIM_NOTIFY*	pNotifies = { nullptr };
	}ANIMATION_DESC;

	typedef struct tagBoneDesc
	{
		char			szName[MAX_PATH] = { };
		int				iParentIndex = { -1 };
		XMFLOAT4X4		TransformationMatrix = {};
	}BONE_DESC;

	typedef struct tagMaterialTextureDesc
	{
		TEXTURE_TYPE	eType;
		wchar_t			szPath[MAX_PATH] = {};
	}MATERIAL_TEXTURE_DESC;

	typedef struct tagMaterialDesc
	{
		unsigned int			iNumTextures	= {};
		MATERIAL_TEXTURE_DESC*	pTextures		= { nullptr };
	}MATERIAL_DESC;

	typedef struct tagModelDesc
	{
		MODEL				eModelType = { MODEL::NONANIM };

		// FBX -> Engine 좌표계 보정 
		XMFLOAT4X4			PreTransformMatrix = {};

		// Root Motion (SLMD v2)
		char				szRootBoneName[MAX_PATH] = {};

		// Mesh
		unsigned int		iNumMeshes = {};
		MESH_DESC*			pMeshes = { nullptr };

		// Material 
		unsigned int		iNumMaterials = {};
		MATERIAL_DESC*		pMaterials = { nullptr };

		// Bone (NonAnim 모델은 0개 가능)
		unsigned int		iNumBones = {};
		BONE_DESC*			pBones = { nullptr };

		// Animation
		unsigned int		iNumAnimations = {};
		ANIMATION_DESC*		pAnimations = { nullptr };

	}MODEL_DESC;

	typedef struct tagActionPolicyBase
	{
		unsigned int	iAction = {};
		unsigned int	iPriority = {};
		bool			bAutoReturn = { false };
		unsigned int	iReturnAction = {};
		float			fCooldown = {};
		float			fEnterBlendTime = {};

	}ACTION_POLICY_BASE;

	typedef struct tagNotifyEvent
	{
		NOTIFY_TYPE eType = { NOTIFY_TYPE::END };
		unsigned int iPayload = {};
		const void* pData = { nullptr };

	}NOTIFY_EVENT;

	typedef struct tagPrototypeInfo
	{
		wstring		strTag;
		PROTOTYPE		eType;
		class CBase*	pPrototype;
	}PROTOTYPE_INFO;

	typedef struct tagSoundSequence
	{
		vector<wstring> SoundKeys;
		unsigned int iCurrentIndex = {};
		float fVolume = { 1.f };
		bool bActive = { false };
	}SOUND_SEQUENCE;
}

#endif // Engine_Struct_h__