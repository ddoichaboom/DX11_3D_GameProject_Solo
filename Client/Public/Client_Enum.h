#ifndef Client_Enum_h__
#define Client_Enum_h__


namespace Client
{
	enum class LEVEL { STATIC, LOADING, LOGO, GAMEPLAY, END };

	enum class CHARACTER_STATE { LOCOMOTION, COMBAT, GUARD, HIT_STAGGER, AIRBORNE, DOWN, STUN, ACTION_LOCKED, DEAD, END };

	enum class WEAPON_TYPE { DEFAULT, DAGGER, SWORD, BOW, PISTOL, POLE_WEAPON, MAGICAL_WEAPON, TWO_HANDED_WEAPON, END };
	
	enum class LOGO_STATE { TITLE, MENU, END };

	enum class MENU_ITEM { START, OPTIONS, QUIT, END };

	enum class EQUIPPED_WEAPON_ID
	{
		NONE,					// 미장착 ( DEFAULT 양손 )

		// DAGGER
		KNIGHT_KILLER,			
		KASAKA_VENOM_FANG,


		// 향후 추가
		END
	};

	enum class CHARACTER_ACTION
	{
		// LOCOMOTION
		IDLE,
		WALK,
		RUN,
		RUN_FAST,
		RUN_FAST_LEFT,
		RUN_FAST_RIGHT,
		DASH,
		BACK_DASH,
		RUN_END,
		RUN_END_LEFT,
		RUN_END_RIGHT,

		// 기본 공격
		BASIC_ATTACK_01,
		BASIC_ATTACK_02,
		BASIC_ATTACK_03,

		// 가드 — STEP 축 사용 (R2 통합, GUARD_START/LOOP/END 폐기)
		GUARD,

		// HIT / FLOAT / DOWN
		FLOAT_A,
		FLOAT_B,
		FLOAT_END,
		DOWN_RECOVERY,
		BREAKFALL,

		UNDRAW,

		WEAPON_SWAP,
		SKILL_F,
		SKILL_Q,
		SKILL_E,
		SKILL_R,
		END
	};

	enum class CHARACTER_ACTION_STEP : unsigned int
	{
		NONE,
		START,
		LOOP,
		END
	};

	enum class CHARACTER_TYPE
	{
		// SungJinWoo
		SUNGJINWOO_ERANK,
		SUNGJINWOO_OVERDRIVE,
		
		END
	};

	enum class SPAWN_TYPE : unsigned int
	{
		PLAYER,
		MONSTER_NORMAL,
		MONSTER_ELITE,
		MONSTER_BOSS,
		END
	};
	//enum class CHARACTER_ACTION { };

	//enum class CHARACTER_ACTION : unsigned int
	//{
	//	// 플레이어 이동 (WASD)
	//	// 기본 상태 
	//	// ( 손에 무기가 부착되어 있는 경우 = 공격/스킬 사용 후 -> UnDraw Weapon 애니메이션 호출 후 IDLE로 복귀)
	//	IDLE,

	//	WALK,			 // 플레이어 이동 입력 직후는 WALK
	//	RUN,			// 이동 입력이 지속될 경우 RUN으로 전환
	//	RUN_FAST,		// DASH 이후의 이동 입력은 RUN_FAST로 진행.

	//	// 회피 (Space)
	//	DASH,
	//	BACK_DASH,

	//	// 기본 공격 - (LMB) 
	//	// 세부 로직은 차차 구현할 예정인데 해당 BASIC_ATTACK의 애니메이션은 현재 장착된/보이고 있는 Weapon Type에 의존 (DAGGER/PISTOL/...)
	//	BASIC_ATTACK_01,
	//	BASIC_ATTACK_02,
	//	BASIC_ATTACK_03,

	//	// 무기 고유 스킬 (C)
	//	CORE_ATTACK,

	//	// 일반 스킬 (Q/E)
	//	// 스킬 트리 - 일반 스킬 세트 A/B 세트에 각각 두개의 스킬들을 매핑해놓는다. 
	//	// 1번 무기 사용 중 - A 세트 -> Core Attack 사용 ->  2번 무기 전환 - B 세트 전환 
	//	NORMAL_SKILL,

	//	// 궁극기 (R)
	//	ULTIMATE,

	//	// 방어 (RMB)
	//	GUARD,
	//	
	//	// 전투 피격/상태이상
	//	BREAKFALL,
	//	HIT,
	//	STUN,
	//	DEATH,

	//	LINK_ATTACK,
	//	CHAIN_SMASH,

	//	END

	//};

	//enum class CHARACTER_TYPE { SUNGJINWOO_OVERDRIVE, END };

	//enum class CHARACTER_CONTROL_TYPE : unsigned int
	//{
	//	PLAYER, 
	//	ALLY_AI,
	//	NPC_AI,

	//	END
	//};

	enum class MONSTER_ACTION : unsigned int
	{
		IDLE,
		WALK,
		RUN,
		CHASE,
		TURN,
		STRAFE_LEFT,
		STRAFE_RIGHT,

		BASIC_ATTACK_01,
		BASIC_ATTACK_02,
		BASIC_ATTACK_03,

		SKILL_01,
		SKILL_02,
		SKILL_03,
		SKILL_04,
		SKILL_05,
		SKILL_06,
		SKILL_07,
		SKILL_08,
		SKILL_09,
		SKILL_10,
		SKILL_11,
		SKILL_12,
		SKILL_13,
		SKILL_14,
		SKILL_15,
		SKILL_16,

		HIT_LIGHT,
		HIT_HEAVY,
		STUN,
		KNOCKDOWN,
		CRASH,
		RECOVER,

		INTRO,
		ROAR,

		DEATH,

		END
	};

	enum class MONSTER_PHASE : unsigned int
	{
		COMMON,
		PHASE_01,
		PHASE_02,
		ENRAGED,

		END
	};

	enum class MONSTER_ACTION_STEP : unsigned int
	{
		NONE,
		START,
		LOOP,
		END,
	};

	enum class MONSTER_ANIM_SET : unsigned int
	{
		NONE,
		IGRIS_BOSS,
		END
	};

	enum class UI_ELEMENT_TYPE : unsigned int
	{
		IMAGE = 0,
		TEXT, 
		SPRITE_ANIM,
		VIDEO,
		END
	};

	enum class UI_SWEEP_MODE 
	{
		NONE = 0,
		POSITION,		// Mesh 자체 X 좌표 좌 -> 우 이동
		UV,				// Mesh 정지, UV Y 스크롤
		END
	};

	enum class UI_TEXT_HALIGN { LEFT, CENTER, RIGHT, END };

	enum class UI_TEXT_VALIGN { TOP, MIDDLE, BOTTOM, END };

	enum class BODY_BLOCK_POLICY
	{
		BLOCK,
		PASS_THROUGH,
		END
	};

	enum class HUD_SLOT
	{
		MONSTER_HP_BACK, MONSTER_HP_REDUCE, MONSTER_HP_FILL, MONSTER_HP_BARLIGHT,
		MONSTER_BREAK_BACK, MONSTER_BREAK_REDUCE, MONSTER_BREAK_FILL,
		PLAYER_HP_BACK, PLAYER_HP_REDUCE, PLAYER_HP_FILL, PLAYER_HP_BARLIGHT,
		PLAYER_MP_BACK, PLAYER_MP_REDUCE, PLAYER_MP_FILL, PLAYER_MP_BARLIGHT,
		DASH_BASE, DASH_LINE,
		DASH_STEP1, DASH_STEP1_GLOW, DASH_STEP2, DASH_STEP2_GLOW, DASH_STEP3, DASH_STEP3_GLOW,
		END
	};

	enum class AREA_ATTACK_SHAPE
	{
		CIRCLE,
		RING,
		FRONT_BOX,
		FRONT_SPHERE,
		END
	};
}
#endif // Client_Enum_h__
