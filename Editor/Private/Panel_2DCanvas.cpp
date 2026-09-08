#include "Panel_2DCanvas.h"
#include "Panel_Manager.h"
#include "Client_Defines.h"
#include "UICanvasTool.h"
#include "GameInstance.h"
#include "Texture.h"

CPanel_2DCanvas::CPanel_2DCanvas(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : CPanel{ pDevice, pContext }
{
}

HRESULT CPanel_2DCanvas::Initialize()
{
    strcpy_s(m_szName, "2D Canvas");

    m_pTool = CUICanvasTool::Create(m_pDevice, m_pContext);
    if (nullptr == m_pTool)
        return E_FAIL;

    return S_OK;
}

void CPanel_2DCanvas::Update(_float fTimeDelta)
{
}

void CPanel_2DCanvas::Render()
{
    ImGui::Begin(m_szName, &m_bOpen);

    if (nullptr == m_pTool)
    {
        ImGui::TextDisabled("UI Canvas tool not found.");
        ImGui::End();
        return;
    }

    _bool bCanvasMode = m_pPanel_Manager->Is_UICanvasMode();
    if (ImGui::Checkbox("UI Canvas Mode", &bCanvasMode))
    {
        m_pPanel_Manager->Set_ToolMode(
            bCanvasMode ? EDITOR_TOOL_MODE::UI_CANVAS : EDITOR_TOOL_MODE::OBJECT);
    }

    ImGui::Separator();

    ImGui::InputText("File", m_szFilePath, sizeof(m_szFilePath));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DND_FILE_PATH))
        {
            const _char* pCStr = static_cast<const _char*>(pPayload->Data);
            strncpy_s(m_szFilePath, sizeof(m_szFilePath), pCStr, _TRUNCATE);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::Button("Save"))
    {
        _tchar szPath[MAX_PATH] = {};
        CharToWChar(m_szFilePath, szPath, MAX_PATH);
        m_pTool->Save(szPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        _tchar szPath[MAX_PATH] = {};
        CharToWChar(m_szFilePath, szPath, MAX_PATH);
        m_pTool->Load(szPath);
    }
    ImGui::SameLine();

    _bool bShowOverlay = m_pTool->Get_ShowOverlayImages();

    if (ImGui::Checkbox("Show Images in Viewport", &bShowOverlay))
        m_pTool->Set_ShowOverlayImages(bShowOverlay);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        m_pTool->Clear();

    ImGui::Separator();

    if (ImGui::Button("Add Image"))      
        m_pTool->Add_Element_Image();
    ImGui::SameLine();
    if (ImGui::Button("Add Text"))       
        m_pTool->Add_Element_Text();
    ImGui::SameLine();
    if (ImGui::Button("Add SpriteAnim")) 
        m_pTool->Add_Element_SpriteAnim();
    ImGui::SameLine();
    if (ImGui::Button("Add Video"))
        m_pTool->Add_Element_Video();

    const _bool bNoSelected = -1 == m_pTool->Get_SelectedIndex();
    if (bNoSelected) ImGui::BeginDisabled();
    if (ImGui::Button("Delete"))        
        m_pTool->Delete_Selected();
    ImGui::SameLine();
    if (ImGui::Button("Duplicate"))     
        m_pTool->Duplicate_Selected();
    ImGui::SameLine();
    if (ImGui::Button("Up"))            
        m_pTool->Move_Selected_Up();
    ImGui::SameLine();
    if (ImGui::Button("Down"))          
        m_pTool->Move_Selected_Down();
    if (bNoSelected) ImGui::EndDisabled();

    ImGui::Separator();
    Render_ElementList();

    ImGui::Separator();
    Render_PropertyEditor();

    ImGui::End();
}

const _char* CPanel_2DCanvas::Get_ElementTypeLabel(UI_ELEMENT_TYPE eType)
{
    switch (eType)
    {
    case UI_ELEMENT_TYPE::IMAGE:            
        return "Image";
    case UI_ELEMENT_TYPE::TEXT:                     
        return "Text";
    case UI_ELEMENT_TYPE::SPRITE_ANIM:      
        return "SpriteAnim";
    case UI_ELEMENT_TYPE::VIDEO:            
        return "Video";
    default:                                                        
        return "Unknown";
    }
}

void CPanel_2DCanvas::Render_ElementList()
{
    ImGui::TextDisabled("Elements (%u)", m_pTool->Get_NumElements());

    ImGui::BeginChild("UIElementList", ImVec2(0.f, 140.f), true);

    for (_uint i = 0; i < m_pTool->Get_NumElements(); ++i)
    {
        UI_ELEMENT* pElement = m_pTool->Get_Element(static_cast<_int>(i));
        if (nullptr == pElement) 
            continue;

        _char szName[MAX_PATH] = {};
        WCharToChar(pElement->szName, szName, MAX_PATH);

        _char szLabel[256] = {};
        sprintf_s(szLabel, "%u. [%s] %s", i, Get_ElementTypeLabel(pElement->eType), szName);

        if (ImGui::Selectable(szLabel, m_pTool->Get_SelectedIndex() == static_cast<_int>(i)))
            m_pTool->Set_SelectedIndex(static_cast<_int>(i));
    }

    ImGui::EndChild();
}

void CPanel_2DCanvas::Render_PropertyEditor()
{
    UI_ELEMENT* pElement = m_pTool->Get_SelectedElement();
    if (nullptr == pElement)
    {
        ImGui::TextDisabled("No element selected.");
        return;
    }

    ImGui::TextDisabled("Properties");
    Render_Common_Properties(pElement);

    switch (pElement->eType)
    {
    case UI_ELEMENT_TYPE::IMAGE:
        Render_Image_Properties(pElement);
        break;
    case UI_ELEMENT_TYPE::TEXT:
        Render_Text_Properties(pElement);
        break;
    case UI_ELEMENT_TYPE::SPRITE_ANIM:
        Render_Image_Properties(pElement);
        Render_SpriteAnim_Properties(pElement);
        break;
    case UI_ELEMENT_TYPE::VIDEO:
        Render_Image_Properties(pElement);
        Render_Video_Properties(pElement);
        break;
    default:
        break;
    }
}

void CPanel_2DCanvas::Render_Common_Properties(UI_ELEMENT* pElement)
{
    _char szName[MAX_PATH] = {};
    WCharToChar(pElement->szName, szName, MAX_PATH);
    if (ImGui::InputText("Name", szName, MAX_PATH))
        CharToWChar(szName, pElement->szName, MAX_PATH);

    _float vCenter[2] = { pElement->fCenterX, pElement->fCenterY };
    if (ImGui::DragFloat2("Center", vCenter, 1.f))
    {
        pElement->fCenterX = vCenter[0];
        pElement->fCenterY = vCenter[1];
    }

    _float vSize[2] = { pElement->fSizeX, pElement->fSizeY };
    if (ImGui::DragFloat2("Size", vSize, 1.f, 1.f, 4096.f))
    {
        pElement->fSizeX = vSize[0];
        pElement->fSizeY = vSize[1];
    }

    ImGui::Text("ZOrder (auto): %u", pElement->iZOrder);

    ImGui::Checkbox("Visible (initial)", &pElement->bVisible);
}

void CPanel_2DCanvas::Render_Image_Properties(UI_ELEMENT* pElement)
{
    _char szPath[MAX_PATH] = {};
    WCharToChar(pElement->szTexturePath, szPath, MAX_PATH);
    if (ImGui::InputText("Texture", szPath, MAX_PATH))
        CharToWChar(szPath, pElement->szTexturePath, MAX_PATH);

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DND_FILE_PATH))
        {
            const _char* pCStr = static_cast<const _char*>(pPayload->Data);
            CharToWChar(pCStr, pElement->szTexturePath, MAX_PATH);
        }
        ImGui::EndDragDropTarget();
    }

    if (UI_ELEMENT_TYPE::VIDEO != pElement->eType)
    {
        ID3D11ShaderResourceView* pPreviewSRV = m_pTool->Get_PreviewSRV(pElement->szTexturePath);        
        if (nullptr != pPreviewSRV)
        {
            constexpr _float fThumbSize = 128.f;
            ImGui::Image(reinterpret_cast<ImTextureID>(pPreviewSRV),
                ImVec2(fThumbSize, fThumbSize));
        }
        else if (0 != pElement->szTexturePath[0])
        {
            ImGui::TextDisabled("(preview unavailable)");
        }
    }

    if (UI_ELEMENT_TYPE::VIDEO == pElement->eType)
        return;

    ImGui::Separator();
    ImGui::TextDisabled("Pattern A (Prototype Pool)");

    // ProtoTag 텍스트 입력
    _char szProtoTag[MAX_PATH] = {};
    WCharToChar(pElement->szTextureProtoTag, szProtoTag, MAX_PATH);
    if (ImGui::InputText("ProtoTag", szProtoTag, MAX_PATH))
        CharToWChar(szProtoTag, pElement->szTextureProtoTag, MAX_PATH);

    static const _char* aLevelNames[] = { "STATIC", "LOADING", "LOGO", "GAMEPLAY" };
    static const _uint  aLevelValues[] = {
        ETOUI(LEVEL::STATIC), ETOUI(LEVEL::LOADING),
        ETOUI(LEVEL::LOGO),   ETOUI(LEVEL::GAMEPLAY)
    };
    constexpr _int iNumLevels = static_cast<_int>(sizeof(aLevelValues) / sizeof(aLevelValues[0]));

    _int iCurrent = 0;
    for (_int i = 0; i < iNumLevels; ++i)
    {
        if (aLevelValues[i] == pElement->iTextureProtoLevel)
        {
            iCurrent = i;
            break;
        }
    }

    if (ImGui::BeginCombo("ProtoLevel", aLevelNames[iCurrent]))
    {
        for (_int i = 0; i < iNumLevels; ++i)
        {
            const _bool bSelected = (i == iCurrent);
            if (ImGui::Selectable(aLevelNames[i], bSelected))
                pElement->iTextureProtoLevel = aLevelValues[i];
            if (bSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const _bool bProtoActive = (0 != pElement->szTextureProtoTag[0]);
    if (bProtoActive)
        ImGui::TextColored(ImVec4(0.6f, 1.f, 0.6f, 1.f), "Pattern A active (path ignored at runtime)");
    else
        ImGui::TextDisabled("Pattern B: path-based (set ProtoTag to use pool)");

    ImGui::Separator();
    ImGui::ColorEdit4("Color (Tint)", &pElement->vColor.x,
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_Float |              
        ImGuiColorEditFlags_AlphaPreviewHalf);

    ImGui::SliderFloat("Alpha##img", &pElement->vColor.w, 0.f, 1.f, "%.2f");

    if (ImGui::SmallButton("White##img"))   
        pElement->vColor = _float4{ 1.f, 1.f, 1.f, pElement->vColor.w };
    ImGui::SameLine();
    if (ImGui::SmallButton("Gold##img"))    
        pElement->vColor = _float4{ 1.f, 0.85f, 0.2f, pElement->vColor.w };
    ImGui::SameLine();
    if (ImGui::SmallButton("Red##img"))     
        pElement->vColor = _float4{ 1.f, 0.3f, 0.3f, pElement->vColor.w };
    ImGui::SameLine();
    if (ImGui::SmallButton("Cyan##img"))    
        pElement->vColor = _float4{ 0.4f, 0.8f, 1.f, pElement->vColor.w };

    ImGui::Separator();
    static const _char* aSweepMode[] = { "None", "Position (X scroll)", "UV (Y scroll)" };
    _int iSweep = ETOI(pElement->eSweepMode);
    if (ImGui::Combo("SweepMode", &iSweep, aSweepMode, IM_ARRAYSIZE(aSweepMode)))
        pElement->eSweepMode = static_cast<UI_SWEEP_MODE>(iSweep);

    if (pElement->eSweepMode != UI_SWEEP_MODE::NONE)
        ImGui::TextColored(ImVec4(0.6f, 1.f, 0.6f, 1.f), "Sweep ON — uses tex alpha as mask + tint color");
}

void CPanel_2DCanvas::Render_Text_Properties(UI_ELEMENT* pElement)
{
    // FontTag 콤보
    vector<_wstring> Tags;
    m_pGameInstance->Get_FontTags(&Tags);

    _char szCurrent[MAX_PATH] = {};
    WCharToChar(pElement->szFontTag, szCurrent, MAX_PATH);

    if (ImGui::BeginCombo("FontTag", szCurrent))
    {
        for (const _wstring& Tag : Tags)
        {
            _char szTag[MAX_PATH] = {};
            WCharToChar(Tag.c_str(), szTag, MAX_PATH);

            const _bool bSelected = (0 == strcmp(szTag, szCurrent));
            if (ImGui::Selectable(szTag, bSelected))
                CharToWChar(szTag, pElement->szFontTag, MAX_PATH);

            if (bSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Text 본문 (UTF-8)
    _char szText[MAX_PATH] = {};
    WCharToChar(pElement->szText, szText, MAX_PATH);
    if (ImGui::InputText("Text", szText, MAX_PATH))
        CharToWChar(szText, pElement->szText, MAX_PATH);

    // === 정렬 ===
    static const _char* aHAlign[] = { "Left", "Center", "Right" };
    static const _char* aVAlign[] = { "Top",  "Middle", "Bottom" };

    _int iH = static_cast<_int>(pElement->eHAlign);
    if (ImGui::Combo("HAlign", &iH, aHAlign, IM_ARRAYSIZE(aHAlign)))
        pElement->eHAlign = static_cast<UI_TEXT_HALIGN>(iH);

    _int iV = static_cast<_int>(pElement->eVAlign);
    if (ImGui::Combo("VAlign", &iV, aVAlign, IM_ARRAYSIZE(aVAlign)))
        pElement->eVAlign = static_cast<UI_TEXT_VALIGN>(iV);

    // === AutoFit ===
    ImGui::Checkbox("Auto Fit (Size box)", &pElement->bAutoFit);
    if (pElement->bAutoFit)
        ImGui::TextDisabled("Scale is clamped to fit Size box");

    ImGui::Separator();
    ImGui::TextDisabled("Text Color");

    ImGui::ColorEdit4("Color##text", &pElement->vColor.x,
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_PickerHueWheel |
        ImGuiColorEditFlags_Float |
        ImGuiColorEditFlags_AlphaPreviewHalf);

    ImGui::SliderFloat("Alpha##text", &pElement->vColor.w, 0.f, 1.f, "%.2f");

    if (ImGui::SmallButton("White##text"))
        pElement->vColor = _float4{ 1.f, 1.f, 1.f, pElement->vColor.w };
    ImGui::SameLine();

    if (ImGui::SmallButton("Gold##text"))
        pElement->vColor = _float4{ 1.f, 0.85f, 0.2f, pElement->vColor.w };
    ImGui::SameLine();

    if (ImGui::SmallButton("Red##text"))
        pElement->vColor = _float4{ 1.f, 0.3f, 0.3f, pElement->vColor.w };
    ImGui::SameLine();

    if (ImGui::SmallButton("CyanBlue##text"))
        pElement->vColor = _float4{ 0.4f, 0.8f, 1.f, pElement->vColor.w };
}

void CPanel_2DCanvas::Render_SpriteAnim_Properties(UI_ELEMENT* pElement)
{
    _int iCols = static_cast<_int>(pElement->iAtlasCols);
    if (ImGui::DragInt("AtlasCols", &iCols, 1, 1, 64))
        pElement->iAtlasCols = static_cast<_uint>(iCols < 1 ? 1 : iCols);

    _int iRows = static_cast<_int>(pElement->iAtlasRows);
    if (ImGui::DragInt("AtlasRows", &iRows, 1, 1, 64))
        pElement->iAtlasRows = static_cast<_uint>(iRows < 1 ? 1 : iRows);

    ImGui::DragFloat("FrameDuration", &pElement->fFrameDuration, 0.001f, 0.001f, 5.f, "%.4f");
}

void CPanel_2DCanvas::Render_Video_Properties(UI_ELEMENT* pElement)
{
    ImGui::Checkbox("Loop", &pElement->bLoop);
    ImGui::DragFloat("PlaybackSpeed", &pElement->fPlaybackSpeed, 0.05f, 0.1f, 4.f, "%.2f");
}

CPanel_2DCanvas* CPanel_2DCanvas::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CPanel_2DCanvas* pInstance = new CPanel_2DCanvas(pDevice, pContext);
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Create : CPanel_2DCanvas");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void CPanel_2DCanvas::Free()
{
    __super::Free();
    Safe_Release(m_pTool);

}
