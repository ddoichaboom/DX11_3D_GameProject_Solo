#pragma once

#include "Client_Defines.h"
#include "PartObject.h"
#include "CharacterAnimTable.h"

NS_BEGIN(Engine)
class CShader;
class CModel;
class CAnimController;
class INotifyListener;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CBody_Player final : public CPartObject
{
public:
	typedef struct tagBodyPlayerDesc : public CPartObject::PARTOBJECT_DESC
	{
		const _uint* pParentState = { nullptr };
	}BODY_PLAYER_DESC;

private:
    CBody_Player(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CBody_Player(const CBody_Player& Prototype);
    virtual ~CBody_Player() = default;

public:
    const _float4x4*                    Get_BoneMatrixPtr(const _char* pBoneName) const;
    _float3                             Get_LastRootMotionDelta() const;

public:
    virtual HRESULT                     Initialize_Prototype() override;
    virtual HRESULT                     Initialize(void* pArg) override;
    virtual void                        Priority_Update(_float fTimeDelta) override;
    virtual void                        Update(_float fTimeDelta) override;
    virtual void                        Late_Update(_float fTimeDelta) override;
    virtual HRESULT                     Render() override;

    virtual HRESULT                     Render_Shadow() override;

public:
    HRESULT                             Play_Action(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep = CHARACTER_ACTION_STEP::NONE);
    void                                Set_Listener(INotifyListener* pListener);
    CHARACTER_ACTION                    Pick_RunEndAction() const;

    void                                Set_EquippedWeaponId(EQUIPPED_WEAPON_ID eId) { m_eEquippedWeaponId = eId; }
    EQUIPPED_WEAPON_ID                  Get_EquippedWeaponId() const { return m_eEquippedWeaponId; }

    void                                Set_WeaponType(WEAPON_TYPE eType) { m_eWeaponState = eType; }
    WEAPON_TYPE                         Get_WeaponType() const { return m_eWeaponState; }
private:
    CShader*                            m_pShaderCom = { nullptr };
    CModel*                             m_pModelCom = { nullptr };
    CAnimController*                    m_pAnimController = { nullptr };

    INotifyListener*                    m_pListener = { nullptr };

private:
    const _uint*                        m_pParentState = { nullptr };
    const CHARACTER_ANIM_TABLE_DESC*    m_pAnimTable = { nullptr };

    CHARACTER_TYPE                      m_eCharacterType = { CHARACTER_TYPE::SUNGJINWOO_OVERDRIVE };
    CHARACTER_STATE                     m_eCurrentState = { CHARACTER_STATE::LOCOMOTION };
    WEAPON_TYPE                         m_eWeaponState = { WEAPON_TYPE::DEFAULT };
    CHARACTER_ACTION                    m_eCurrentAction = { CHARACTER_ACTION::IDLE };

    CHARACTER_ACTION_STEP               m_eCurrentStep = { CHARACTER_ACTION_STEP::NONE };
    EQUIPPED_WEAPON_ID                  m_eEquippedWeaponId = { EQUIPPED_WEAPON_ID::NONE };

private:
    HRESULT                             Ready_Components();
    HRESULT                             Bind_ShaderResources();
    HRESULT                             Bind_ShadowResources();

    HRESULT                             Ready_AnimationTable();
    HRESULT                             Register_AnimationClips();

    const CHARACTER_ANIM_BIND_DESC*     Find_Bind(CHARACTER_STATE eState, CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep, WEAPON_TYPE  eWeapon, EQUIPPED_WEAPON_ID eEquippedId) const;
    _uint64                             Resolve_ActionKey(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eSte = CHARACTER_ACTION_STEP::NONE);
    const CHARACTER_ACTION_POLICY*      Find_ActionPolicy(CHARACTER_ACTION eAction, CHARACTER_ACTION_STEP eStep = CHARACTER_ACTION_STEP::NONE) const;

public:
    static CBody_Player*                Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CGameObject*                Clone(void* pArg) override;
    virtual void                        Free() override;
};

NS_END