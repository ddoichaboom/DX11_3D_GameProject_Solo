#ifndef Engine_Enum_h__
#define Engine_Enum_h__

namespace Engine
{
	enum class WINMODE { FULL, WIN, END };

	enum class PROTOTYPE { GAMEOBJECT, COMPONENT, END };

	enum class TRANSFORMTYPE { TRANSFORM_2D, TRANSFORM_3D, END };

	enum class RENDERID { PRIORITY, NONBLEND, NONLIGHT, BLEND, UI, END };

    enum class DEFERRED { DEBUG, DIRECTIONAL, POINT, COMBINED, END };

	enum class STATE { RIGHT, UP, LOOK, POSITION, END };

	enum class D3DTS { VIEW, PROJ, END };

	enum class MOUSEBTN { LBUTTON, RBUTTON, MBUTTON, END };

	enum class MOUSEAXIS { X, Y, WHEEL, END };

	enum class LIGHT { DIRECTIONAL, POINT, END };

	enum class MODEL { NONANIM, ANIM, END };

    enum class TEXTURE_TYPE
    {
        DIFFUSE,
        SPECULAR,
        AMBIENT,
        EMISSIVE,
        HEIGHT,
        NORMAL,
        SHININESS,
        OPACITY,
        DISPLACEMENT,
        LIGHTMAP,
        REFLECTION,
        BASE_COLOR,
        NORMAL_CAMERA,
        EMISSION_COLOR,
        METALNESS,
        DIFFUSE_ROUGHNESS,
        AMBIENT_OCCLUSION,

        UNKNOWN,
        END
    };

    enum class NOTIFY_TYPE : unsigned int
    {
        ACTION_FINISHED,
        ANIM_EVENT,

        // 향후 확장

        END
    };

    enum class ANIM_NOTIFY_TYPE : unsigned int
    {
        NONE = 0,
        FOOTSTEP_L,
        FOOTSTEP_R,
        ATTACK_HIT,
        COMBO_WINDOW_OPEN,
        COMBO_WINDOW_CLOSE,
        ATTACK_HITBOX_ON,
        ATTACK_HITBOX_OFF,
        DETECT_ON,
        DETECT_OFF,
        INVINCIBLE_ON,
        INVINCIBLE_OFF,
        END
    };

    enum class COLLIDER : unsigned int
    {
        AABB, 
        OBB,
        SPHERE,
        END
    };

    enum class COLLISION_GROUP : unsigned int
    {
        PLAYER_BODY,
        PLAYER_ATTACK,
        MONSTER_BODY,
        MONSTER_ATTACK,
        MAP_STATIC,
        END
    };


}
#endif // Engine_Enum_h__
