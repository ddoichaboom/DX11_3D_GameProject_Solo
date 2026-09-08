#ifndef Client_Func_h
#define Client_Func_h

namespace Client
{
	inline _uint64 Make_CharacterAnimKey(CHARACTER_STATE eState, CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep,	WEAPON_TYPE eWeapon, EQUIPPED_WEAPON_ID eEquippedId = EQUIPPED_WEAPON_ID::NONE)
	{
		return (ETOUI64(eState) << 56) |
			(ETOUI64(eAction) << 48) |
			(ETOUI64(eStep) << 40) |
			(ETOUI64(eWeapon) << 32) |
			(ETOUI64(eEquippedId) << 24);
	}

	inline _uint Make_PlayerStateKey(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep = CHARACTER_ACTION_STEP::NONE)
	{
		return (ETOUI(eAction) << 8) | ETOUI(eStep);
	}

	inline CHARACTER_ACTION Get_PlayerActionFromStateKey(_uint iKey)
	{
		return static_cast<CHARACTER_ACTION>(iKey >> 8);
	}

	inline CHARACTER_ACTION_STEP Get_PlayerStepFromStateKey(_uint iKey)
	{
		return static_cast<CHARACTER_ACTION_STEP>(iKey & 0xff);
	}

	inline _uint64 Make_MonsterAnimKey(MONSTER_ACTION eAction, MONSTER_PHASE ePhase, MONSTER_ACTION_STEP eStep)
	{
		return (ETOUI64(eAction) << 32) |
			(ETOUI64(ePhase) << 16) |
			ETOUI64(eStep);
	}

	inline _uint Make_MonsterStateKey(MONSTER_ACTION eAction, MONSTER_ACTION_STEP eStep = MONSTER_ACTION_STEP::NONE)
	{
		return (ETOUI(eAction) << 8) | ETOUI(eStep);
	}

	inline MONSTER_ACTION Get_MonsterActionFromStateKey(_uint iKey)
	{
		return static_cast<MONSTER_ACTION>(iKey >> 8);
	}

	inline MONSTER_ACTION_STEP Get_MonsterStepFromStateKey(_uint iKey)
	{
		return static_cast<MONSTER_ACTION_STEP>(iKey & 0xff);
	}

	inline _bool Has_MoveIntent(const PLAYER_INTENT_FRAME& Intent)
	{
		const _float x = Intent.vMoveDirWorld.x;
		const _float z = Intent.vMoveDirWorld.z;

		return (x * x + z * z) > 0.0001f;
	}
}

#endif // Client_Func_h