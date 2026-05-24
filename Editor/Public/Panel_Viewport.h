#pragma once

#include "Editor_Defines.h"
#include "Panel.h"

NS_BEGIN(Engine)
class CGameObject;
NS_END

NS_BEGIN(Editor)

class CPanel_Viewport final : public CPanel
{
private:
    CPanel_Viewport(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual ~CPanel_Viewport() = default;

public:
    virtual HRESULT             Initialize() override;
    virtual void                Update(_float fTimeDelta) override;
    virtual void                Render() override;

#pragma region CAMERA
public:
    void                        Resize_ActiveCameraProjection();
#pragma endregion

#pragma region RENDER_TARGET
public:
    HRESULT                     Begin_RT(); 
    HRESULT                     End_RT();

    // RT Á¢±ÙÀÚ
    ID3D11ShaderResourceView*   Get_SRV() const;
    _uint                       Get_RTWidth() const { return m_iRTWidth; }
	_uint                       Get_RTHeight() const { return m_iRTHeight; }

private:
    _uint                       m_iRTWidth = { 0 };
	_uint                       m_iRTHeight = { 0 };

#pragma endregion

#pragma region PICKING
private:
    typedef struct tagPickResult
    {
        CGameObject*            pObject = { nullptr };
        _float3			        vPosition = {};
        _float			        fDistance = {};
    }PICK_RESULT;

    void                        Pick_Object();
    _bool                       Pick_Surface(PICK_RESULT* pOutResult, _bool bMapOnly = false);

    _float                      m_fPickX = {};
    _float                      m_fPickY = {};
#pragma endregion

#pragma region GIZMO

private:
    ImGuizmo::OPERATION         m_eGizmoOperation   = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE              m_eGizmoMode        = ImGuizmo::LOCAL;

    _float                      m_fSnapTranslate    = { 1.0f };
    _float                      m_fSnapRotate       = { 15.0f };
    _float                      m_fSnapScale        = { 0.1f };
#pragma endregion

#pragma region UICANVAS
private:
    class CUICanvasTool*        Find_UICanvasTool();
#pragma endregion

public:
    static CPanel_Viewport*     Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    virtual void                Free() override;

};

NS_END