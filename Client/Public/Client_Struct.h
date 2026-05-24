#ifndef Client_Struct_h
#define Client_Struct_h

#include "NavMesh_Types.h"

namespace Client
{
	typedef struct tagCharacterAnimBindDesc
	{
		CHARACTER_STATE				eState			= { CHARACTER_STATE::END };
		CHARACTER_ACTION			eAction			= { CHARACTER_ACTION::END };
		CHARACTER_ACTION_STEP		eStep			= { CHARACTER_ACTION_STEP::NONE };
		WEAPON_TYPE					eWeapon			= { WEAPON_TYPE::END };
		EQUIPPED_WEAPON_ID			eEquippedId		= { EQUIPPED_WEAPON_ID::NONE };
		const _char*				pAnimationName	= { nullptr };
		_bool						bRestartOnEnter = { true };
	}CHARACTER_ANIM_BIND_DESC;

	typedef struct tagCharacterActionPolicy
	{
		CHARACTER_ACTION			eAction = { CHARACTER_ACTION::END };
		CHARACTER_ACTION_STEP		eStep = { CHARACTER_ACTION_STEP::NONE };
		_uint						iPriority = {};
		_bool						bAutoReturn = { false };
		CHARACTER_ACTION			eReturnAction = { CHARACTER_ACTION::IDLE };
		CHARACTER_ACTION_STEP		eReturnStep = { CHARACTER_ACTION_STEP::NONE };
		_float						fEnterBlendTime = { 0.f };
		_float						fSphereRadius = { 0.f };
		_float						fSphereDamage = { 0.f };
		_float						fSphereForwardOffset = { 0.f };
	} CHARACTER_ACTION_POLICY;

	typedef struct tagCharacterAnimTableDesc
	{
		CHARACTER_TYPE						eCharacterType		= { CHARACTER_TYPE::END };
		const _tchar*						pModelPrototypeTag	= { nullptr };
		const _char*						pRootBoneName		= { nullptr };

		const CHARACTER_ANIM_BIND_DESC*		pBinds				= { nullptr };
		_uint								iNumBinds			= {};

		const CHARACTER_ACTION_POLICY*		pPolicies			= { nullptr };
		_uint								iNumPolicies		= {};
	} CHARACTER_ANIM_TABLE_DESC;

	typedef struct tagMonsterAnimBindDesc
	{
		MONSTER_ACTION                  eAction = { MONSTER_ACTION::END };
		MONSTER_PHASE                   ePhase = { MONSTER_PHASE::COMMON };
		MONSTER_ACTION_STEP             eStep = { MONSTER_ACTION_STEP::NONE };
		const _char*					pAnimationName = { nullptr };
		_bool                           bRestartOnEnter = { true };
		_bool                           bOverrideLoop = { false };
		_bool                           bLoop = { false };
		_bool                           bOverrideRootMotion = { false };
		_bool                           bUseRootMotion = { false };
	} MONSTER_ANIM_BIND_DESC;

	typedef struct tagMonsterActionPolicy
	{
		MONSTER_ACTION                  eAction = { MONSTER_ACTION::END };
		MONSTER_ACTION_STEP             eStep = { MONSTER_ACTION_STEP::NONE };

		_uint							iPriority = {};
		_bool                           bAutoReturn = { false };

		MONSTER_ACTION                  eReturnAction = { MONSTER_ACTION::IDLE };
		MONSTER_ACTION_STEP             eReturnStep = { MONSTER_ACTION_STEP::NONE };

		_float                          fCooldown = { 0.f };
		_float                          fEnterBlendTime = { 0.f };
	} MONSTER_ACTION_POLICY;

	typedef struct tagMonsterAnimTableDesc
	{
		MONSTER_ANIM_SET				eAnimSet = { MONSTER_ANIM_SET::END };
		const _tchar*					pModelPrototypeTag = { nullptr };
		const _char*					pRootBoneName = { nullptr };

		const MONSTER_ANIM_BIND_DESC*	pClips = { nullptr };
		_uint							iNumClips = {};

		const MONSTER_ACTION_POLICY*	pPolicies = { nullptr };
		_uint							iNumPolicies = {};
	} MONSTER_ANIM_TABLE_DESC;

	typedef struct tagPlayerRawInputFrame
	{
		_bool bMoveForwardHeld = { false };
		_bool bMoveBackwardHeld = { false };
		_bool bMoveLeftHeld = { false };
		_bool bMoveRightHeld = { false };

		_bool bRButtonHeld = { false };
		_bool bLButtonPressed = { false };
		_bool bDashPressed = { false };

		_bool bWeaponSwapPressed = { false };
		_bool bSkillFPressed = { false };
		_bool bSkillQPressed = { false };
		_bool bSkillEPressed = { false };
		_bool bShiftPressed = { false };

		_long lMouseDeltaX = {};
		_long lMouseDeltaY = {};
	}PLAYER_RAW_INPUT_FRAME;

	typedef struct tagPlayerIntentFrame
	{
		_float3 vMoveDirWorld = {};
		_long	lLookDeltaX = {};

		_bool bDashRequested = { false };		// �̹� �����ӿ� Dash ��û
		_bool bAttackRequested = { false };
		_bool bSkillFRequested = { false };
		_bool bSkillQRequested = { false };
		_bool bSkillERequested = { false };
		_bool bQTERequested = { false };

		_bool bGuardHeld = { false };

		_bool bWeaponSwapRequested = { false };

	}PLAYER_INTENT_FRAME;

	typedef struct tagSpawnPoint
	{
		SPAWN_TYPE		eType = { SPAWN_TYPE::END };
		_float3			vPosition = {};
		_float3			vRotationDeg = {};
		_int			iNavCellIndex = { INVALID_INDEX };
		_tchar			szName[MAX_PATH] = {};

		_int			iLevel = { 1 };
		_tchar			szDisplayName[MAX_PATH] = {};
	}SPAWN_POINT;

	typedef struct tagSceneLight
	{
		LIGHT                   eType = { LIGHT::DIRECTIONAL };
		_tchar					szName[MAX_PATH] = {};

		_float4                 vDiffuse = {};
		_float4                 vAmbient = {};
		_float4                 vSpecular = {};

		_float4                 vDirection = {};
		_float4                 vPosition = {};
		_float                  fRange = {};
	}SCENE_LIGHT;

	typedef struct tagSceneData
	{
		_tchar					szNavDataPath[MAX_PATH] = { };
		vector<SPAWN_POINT>		SpawnPoints;
		vector<SCENE_LIGHT>     SceneLights;
	}SCENE_DATA;

	typedef struct tagUIElemet
	{
		UI_ELEMENT_TYPE		eType = { UI_ELEMENT_TYPE::IMAGE };
		_tchar              szName[MAX_PATH] = {};
		_tchar              szTexturePath[MAX_PATH] = {};

		_tchar				szTextureProtoTag[MAX_PATH] = {};
		_uint				iTextureProtoLevel = { 0 };

		_float              fCenterX = { 0.f };
		_float              fCenterY = { 0.f };
		_float              fSizeX = { 100.f };
		_float              fSizeY = { 100.f };
		_uint               iZOrder = { 0 };

		_tchar				szFontTag[MAX_PATH] = {};
		_tchar				szText[MAX_PATH] = {};
		_uint				iAtlasCols = { 1 };
		_uint				iAtlasRows = { 1 };
		_float				fFrameDuration = { 0.1f };

		_bool				bLoop = { true };
		_float				fPlaybackSpeed = { 1.f };

		UI_TEXT_HALIGN		eHAlign = UI_TEXT_HALIGN::CENTER;
		UI_TEXT_VALIGN		eVAlign = UI_TEXT_VALIGN::MIDDLE;
		_bool				bAutoFit = false;
		_bool				bVisible = { true };

		_float4				vColor = { 1.f, 1.f, 1.f, 1.f };
		UI_SWEEP_MODE		eSweepMode = { UI_SWEEP_MODE::NONE };
	}UI_ELEMENT;

	typedef struct tagUISceneData
	{
		_tchar                  szName[MAX_PATH] = {};
		_float                  fAuthoringWidth = { 1280.f };
		_float                  fAuthoringHeight = { 720.f };
		vector<UI_ELEMENT>      Elements;
	}UI_SCENE_DATA;

	typedef struct tagWeaponInfo
	{
		EQUIPPED_WEAPON_ID	eId;
		WEAPON_TYPE			eCategory;
		const _tchar*		pModelTag;
	}WEAPON_INFO;

	typedef struct tagAreaAttackDesc
	{
		AREA_ATTACK_SHAPE             eShape = { AREA_ATTACK_SHAPE::CIRCLE };

		_float                        fInnerRadius = { 0.f };
		_float                        fOuterRadius = { 0.f };
		_float                        fHeight = { 3.f };

		_float3                       vOffset = {};
		_float3                       vBoxHalfExtents = {};

		_float                        fDamage = { 0.f };
		_float                        fFillDuration = { 0.f };

		_bool                         bClearHitTargetsOnBegin = { true };
	}AREA_ATTACK_DESC;
}

#endif // Client_Struct_h