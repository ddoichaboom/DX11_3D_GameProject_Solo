#pragma once
#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class CShader;
class CTexture;
class CVIBuffer_Rect_Instance;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL CDamageFont final : public CGameObject
{
public:
    typedef struct tagDamageFontDesc : public CGameObject::GAMEOBJECT_DESC
    {
        const _tchar* pTextureProtoTag = { nullptr };
        _uint                   iAtlasCols = { 10 };
        _uint                   iAtlasRows = { 1 };
        _uint                   iMaxInstanceCount = { 128 };
    }DAMAGEFONT_DESC;

private:
    CDamageFont(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    CDamageFont(const CDamageFont& Prototype);
    virtual ~CDamageFont() = default;

public:
    static CDamageFont* Get_Instance() { return s_pInstance; }
    void                                            Spawn(const _float3& vWorldCenter, _int iValue, _bool bCrit = false);

public:
    virtual HRESULT                         Initialize_Prototype() override;
    virtual HRESULT                         Initialize(void* pArg) override;
    virtual void                            Update(_float fTimeDelta) override;
    virtual void                            Late_Update(_float fTimeDelta) override;
    virtual HRESULT                         Render() override;

private:
    HRESULT                                         Ready_Components(const DAMAGEFONT_DESC* pDesc);

private:
    static CDamageFont* s_pInstance;

    struct DAMAGE_ENTRY
    {
        _float3         vBasePos;
        _int            iValue;
        _float          fAge;
        _float          fLifetime;
        _bool           bCrit;
    };
    vector<DAMAGE_ENTRY>            m_Entries;

    CShader* m_pShaderCom = { nullptr };
    CTexture* m_pTextureCom = { nullptr };
    CVIBuffer_Rect_Instance* m_pVIBufferCom = { nullptr };

    _wstring                                        m_strTextureProtoTag;
    _uint                                           m_iAtlasCols = { 10 };
    _uint                                           m_iAtlasRows = { 1 };
    _uint                                           m_iMaxInstanceCount = { 128 };

    static constexpr _float         LIFETIME = { 0.7f };
    static constexpr _float         RISE_DIST = { 0.55f };
    static constexpr _float         SPHERE_RADIUS = { 0.45f };
    static constexpr _float         DIGIT_SIZE_X = { 0.15f };
    static constexpr _float         DIGIT_SIZE_Y = { 0.27f };
    static constexpr _float         DIGIT_SPACING = { 0.18f };
    static constexpr _float         POP_TIME = { 0.14f };       
    static constexpr _float         POP_SCALE_MAX = { 1.25f };
    static constexpr _float         CRIT_SCALE = { 1.5f };

public:
    static CDamageFont* Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual CGameObject* Clone(void* pArg) override;
    virtual void                            Free() override;
};

NS_END

