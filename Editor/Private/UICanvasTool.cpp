#include "UICanvasTool.h"
#include "UISceneSerializer.h"
#include "Texture.h"
#include "GameInstance.h"

CUICanvasTool::CUICanvasTool(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
    : m_pDevice { pDevice }
    , m_pContext{ pContext }
{
    Safe_AddRef(m_pDevice);
    Safe_AddRef(m_pContext);
}

_int CUICanvasTool::Add_Element_Image()
{
    return Add_Element(UI_ELEMENT_TYPE::IMAGE);
}

_int CUICanvasTool::Add_Element_Text()
{
    return Add_Element(UI_ELEMENT_TYPE::TEXT);
}

_int CUICanvasTool::Add_Element_SpriteAnim()
{
    return Add_Element(UI_ELEMENT_TYPE::SPRITE_ANIM);
}

_int CUICanvasTool::Add_Element_Video()
{
    return Add_Element(UI_ELEMENT_TYPE::VIDEO);
}

HRESULT CUICanvasTool::Delete_Selected()
{
    if (m_iSelectedIndex < 0 || m_iSelectedIndex >= static_cast<_int>(m_SceneData.Elements.size()))
        return E_FAIL;

    m_SceneData.Elements.erase(m_SceneData.Elements.begin() + m_iSelectedIndex);
    Reindex_ZOrders();
    if (m_SceneData.Elements.empty())
        m_iSelectedIndex = -1;
    else if (m_iSelectedIndex >= static_cast<_int>(m_SceneData.Elements.size()))
        m_iSelectedIndex = static_cast<_int>(m_SceneData.Elements.size()) - 1;
    return S_OK;
}

HRESULT CUICanvasTool::Duplicate_Selected()
{
    UI_ELEMENT* pSrc = Get_SelectedElement();
    if (nullptr == pSrc)
        return E_FAIL;

    UI_ELEMENT Copy = *pSrc;
    _tchar szName[MAX_PATH] = {};
    swprintf_s(szName, TEXT("%s_copy%u"), pSrc->szName, m_iNameCounter++);
    wcscpy_s(Copy.szName, szName);
    Copy.iZOrder = static_cast<_uint>(m_SceneData.Elements.size());

    m_SceneData.Elements.push_back(Copy);
    Reindex_ZOrders();
    m_iSelectedIndex = static_cast<_int>(m_SceneData.Elements.size()) - 1;
    return S_OK;
}

HRESULT CUICanvasTool::Move_Selected_Up()
{
    if (m_iSelectedIndex <= 0)
        return E_FAIL;
    swap(m_SceneData.Elements[m_iSelectedIndex], m_SceneData.Elements[m_iSelectedIndex - 1]);
    Reindex_ZOrders();
    --m_iSelectedIndex;
    return S_OK;
}

HRESULT CUICanvasTool::Move_Selected_Down()
{
    if (m_iSelectedIndex < 0 || m_iSelectedIndex + 1 >= static_cast<_int>(m_SceneData.Elements.size()))
        return E_FAIL;
    swap(m_SceneData.Elements[m_iSelectedIndex], m_SceneData.Elements[m_iSelectedIndex + 1]);
    Reindex_ZOrders();
    ++m_iSelectedIndex;
    return S_OK;
}

HRESULT CUICanvasTool::Save(const _tchar* pFilePath)
{
    if (nullptr == pFilePath || 0 == pFilePath[0])
    {
        Log_Message(LOG_LEVEL::ERROR_, "[UI] Save: invalid path.");
        return E_FAIL;
    }

    if (FAILED(CUISceneSerializer::Save(pFilePath, m_SceneData)))
    {
        _char szPath[MAX_PATH] = {};
        WCharToChar(pFilePath, szPath, MAX_PATH);
        Log_Message(LOG_LEVEL::ERROR_, std::string("[UI] Save failed: ") + szPath);
        return E_FAIL;
    }

    _char szPath[MAX_PATH] = {};
    WCharToChar(pFilePath, szPath, MAX_PATH);
    _char szMsg[MAX_PATH + 64] = {};
    sprintf_s(szMsg, "[UI] Saved %u elements -> %s",
        static_cast<_uint>(m_SceneData.Elements.size()), szPath);
    Log_Message(LOG_LEVEL::INFO, szMsg);
    return S_OK;
}

HRESULT CUICanvasTool::Load(const _tchar* pFilePath)
{
    if (nullptr == pFilePath || 0 == pFilePath[0])
    {
        Log_Message(LOG_LEVEL::ERROR_, "[UI] Load: invalid path.");
        return E_FAIL;
    }

    UI_SCENE_DATA Loaded = {};
    if (FAILED(CUISceneSerializer::Load(pFilePath, &Loaded)))
    {
        _char szPath[MAX_PATH] = {};
        WCharToChar(pFilePath, szPath, MAX_PATH);
        Log_Message(LOG_LEVEL::ERROR_, std::string("[UI] Load failed: ") + szPath);
        return E_FAIL;
    }

    m_SceneData = Loaded;
    m_iSelectedIndex = m_SceneData.Elements.empty() ? -1 : 0;

    _char szPath[MAX_PATH] = {};
    WCharToChar(pFilePath, szPath, MAX_PATH);
    _char szMsg[MAX_PATH + 64] = {};
    sprintf_s(szMsg, "[UI] Loaded %u elements <- %s",
        static_cast<_uint>(m_SceneData.Elements.size()), szPath);
    Log_Message(LOG_LEVEL::INFO, szMsg);
    return S_OK;
}

void CUICanvasTool::Clear()
{
    const _uint iPrev = static_cast<_uint>(m_SceneData.Elements.size());
    m_SceneData.Elements.clear();
    m_iSelectedIndex = -1;

    _char szMsg[64] = {};
    sprintf_s(szMsg, "[UI] Cleared (%u elements).", iPrev);
    Log_Message(LOG_LEVEL::INFO, szMsg);
}

UI_ELEMENT* CUICanvasTool::Get_Element(_int iIndex)
{
    if (iIndex < 0 || iIndex >= static_cast<_int>(m_SceneData.Elements.size()))
        return nullptr;
    return &m_SceneData.Elements[iIndex];
}

UI_ELEMENT* CUICanvasTool::Get_SelectedElement()
{
    return Get_Element(m_iSelectedIndex);
}

void CUICanvasTool::Set_SelectedIndex(_int iIndex)
{
    if (iIndex < -1 || iIndex >= static_cast<_int>(m_SceneData.Elements.size()))
        return;
    m_iSelectedIndex = iIndex;
}

void CUICanvasTool::Render_Overlay(const ImVec2& vImagePos, _uint iViewportW, _uint iViewportH)
{
    ImDrawList* pDraw = ImGui::GetWindowDrawList();

    // 1) 캔버스 외곽 (cyan)
    const ImVec2 vCanvasTL = Canvas_To_Screen(0.f, 0.f, vImagePos, iViewportW, iViewportH);
    const ImVec2 vCanvasBR = Canvas_To_Screen(m_SceneData.fAuthoringWidth, m_SceneData.fAuthoringHeight, vImagePos, iViewportW, iViewportH);
    pDraw->AddRect(vCanvasTL, vCanvasBR, IM_COL32(0, 200, 255, 200), 0.f, 0, 2.f);

    // 2) 엘리먼트들
    for (_int i = 0; i < static_cast<_int>(m_SceneData.Elements.size()); ++i)
    {
        const UI_ELEMENT& E = m_SceneData.Elements[i];
        const _float fHalfX = E.fSizeX * 0.5f;
        const _float fHalfY = E.fSizeY * 0.5f;
        const ImVec2 vTL = Canvas_To_Screen(E.fCenterX - fHalfX, E.fCenterY - fHalfY, vImagePos, iViewportW, iViewportH);
        const ImVec2 vBR = Canvas_To_Screen(E.fCenterX + fHalfX, E.fCenterY + fHalfY, vImagePos, iViewportW, iViewportH);

        if (m_bShowOverlayImages &&
            (UI_ELEMENT_TYPE::IMAGE == E.eType || UI_ELEMENT_TYPE::SPRITE_ANIM == E.eType))
        {
            if (ID3D11ShaderResourceView* pSRV = Get_PreviewSRV(E.szTexturePath))
            {
                ImVec2 uv0(0.f, 0.f), uv1(1.f, 1.f);
                if (UI_ELEMENT_TYPE::SPRITE_ANIM == E.eType)
                {
                    const _uint iCols = (E.iAtlasCols < 1 ? 1 : E.iAtlasCols);
                    const _uint iRows = (E.iAtlasRows < 1 ? 1 : E.iAtlasRows);
                    uv1.x = 1.f / static_cast<_float>(iCols);
                    uv1.y = 1.f / static_cast<_float>(iRows);
                }
                pDraw->AddImage(reinterpret_cast<ImTextureID>(pSRV), vTL, vBR, uv0, uv1,
                    IM_COL32(255, 255, 255, 220));
            }
        }

        const _bool bSel = (i == m_iSelectedIndex);
        const ImU32 cBorder = bSel ? IM_COL32(255, 220, 0, 255) : IM_COL32(180, 180, 180, 180);
        const ImU32 cFill = bSel ? IM_COL32(255, 220, 0, 30) : IM_COL32(180, 180, 180, 15);

        pDraw->AddRectFilled(vTL, vBR, cFill);
        pDraw->AddRect(vTL, vBR, cBorder, 0.f, 0, bSel ? 2.f : 1.f);

        if (bSel)
        {
            const ImVec2 vTR = ImVec2(vBR.x, vTL.y);
            const ImVec2 vBL = ImVec2(vTL.x, vBR.y);
            const _float fH = 5.f;
            const ImU32 cHandle = IM_COL32(255, 255, 255, 255);
            const ImU32 cHandleBorder = IM_COL32(255, 100, 0, 255);

            auto DrawHandle = [&](const ImVec2& p)
                {
                    pDraw->AddRectFilled(ImVec2(p.x - fH, p.y - fH), ImVec2(p.x + fH, p.y + fH), cHandle);
                    pDraw->AddRect(ImVec2(p.x - fH, p.y - fH), ImVec2(p.x + fH, p.y + fH), cHandleBorder, 0.f, 0, 1.f);
                };

            DrawHandle(vTL);
            DrawHandle(vTR);
            DrawHandle(vBL);
            DrawHandle(vBR);
        }
    }
}

void CUICanvasTool::Handle_Interaction(const ImVec2& vImagePos, _uint iViewportW, _uint iViewportH, _bool bImageHovered)
{
    const ImVec2 vMouse = ImGui::GetMousePos();

    if (DRAG_MODE::NONE == m_eDragMode)
    {
        if (bImageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            DRAG_MODE eMode = DRAG_MODE::NONE;
            _int iHit = Pick_Element(vMouse, vImagePos, iViewportW, iViewportH, &eMode);
            if (iHit >= 0)
            {
                m_iSelectedIndex = iHit;
                m_eDragMode = eMode;
                UI_ELEMENT* pE = Get_SelectedElement();
                if (nullptr != pE)
                {
                    m_vDragStart = vMouse;
                    m_fStartCX = pE->fCenterX;
                    m_fStartCY = pE->fCenterY;
                    m_fStartSX = pE->fSizeX;
                    m_fStartSY = pE->fSizeY;
                }
            }
            else
            {
                m_iSelectedIndex = -1;
            }
        }
    }
    else
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_eDragMode = DRAG_MODE::NONE;
        }
        else
        {
            const ImVec2 vDelta = ImVec2(vMouse.x - m_vDragStart.x, vMouse.y - m_vDragStart.y);
            Apply_Drag(vDelta, iViewportW, iViewportH);
        }
    }
}

void CUICanvasTool::Render_TextPreview_ToRT(_uint iRTWidth, _uint iRTHeight)
{
    if (m_SceneData.Elements.empty()) return;

    CGameInstance* pInstance = CGameInstance::GetInstance();
    if (nullptr == pInstance) return;

    const _float fScaleX = static_cast<_float>(iRTWidth) / m_SceneData.fAuthoringWidth;
    const _float fScaleY = static_cast<_float>(iRTHeight) / m_SceneData.fAuthoringHeight;

    for (const UI_ELEMENT& E : m_SceneData.Elements)
    {
        if (UI_ELEMENT_TYPE::TEXT != E.eType) continue;
        if (0 == E.szText[0] || 0 == E.szFontTag[0]) continue;

        // 1. 텍스트 네이티브 크기 측정 (스케일 1)
        _float2 vTextSize{};
        if (FAILED(pInstance->Measure_Font(E.szFontTag, E.szText, &vTextSize)))
            continue;

        // 2. AutoFit 시 박스 비율로 축소 (canvas 좌표 기준)
        _float fFinalScale = 1.f;   // 런타임 UI_Text 의 m_fScale 디폴트
        if (E.bAutoFit && vTextSize.x > 0.f && vTextSize.y > 0.f &&
            E.fSizeX > 0.f && E.fSizeY > 0.f)
        {
            const _float fFitX = E.fSizeX / vTextSize.x;
            const _float fFitY = E.fSizeY / vTextSize.y;
            fFinalScale = (fFitX < fFitY) ? fFitX : fFitY;
        }

        const _float fDrawW = vTextSize.x * fFinalScale;   // canvas 단위
        const _float fDrawH = vTextSize.y * fFinalScale;

        // 3. 박스 좌상단 (canvas)
        const _float fBoxLeft = E.fCenterX - E.fSizeX * 0.5f;
        const _float fBoxTop = E.fCenterY - E.fSizeY * 0.5f;

        // 4. 정렬 오프셋 (canvas)
        _float fOffsetX = 0.f;
        switch (E.eHAlign)
        {
        case UI_TEXT_HALIGN::LEFT:   fOffsetX = 0.f; break;
        case UI_TEXT_HALIGN::CENTER: fOffsetX = (E.fSizeX - fDrawW) * 0.5f; break;
        case UI_TEXT_HALIGN::RIGHT:  fOffsetX = E.fSizeX - fDrawW; break;
        default: break;
        }
        _float fOffsetY = 0.f;
        switch (E.eVAlign)
        {
        case UI_TEXT_VALIGN::TOP:    fOffsetY = 0.f; break;
        case UI_TEXT_VALIGN::MIDDLE: fOffsetY = (E.fSizeY - fDrawH) * 0.5f; break;
        case UI_TEXT_VALIGN::BOTTOM: fOffsetY = E.fSizeY - fDrawH; break;
        default: break;
        }

        // 5. canvas → viewport 좌표 변환
        const _float2 vPos((fBoxLeft + fOffsetX) * fScaleX,
            (fBoxTop + fOffsetY) * fScaleY);

        // 6. 스케일도 viewport 환산 (canvas->RT)
        // X/Y 다른 비율이면 평균 또는 최소값 사용 — 일반적으로 같으니 평균
        const _float2 vRenderScale(fFinalScale * fScaleX, fFinalScale * fScaleY);

        _float4 vPremul{};
        vPremul.x = E.vColor.x * E.vColor.w;
        vPremul.y = E.vColor.y * E.vColor.w;
        vPremul.z = E.vColor.z * E.vColor.w;
        vPremul.w = E.vColor.w;

        pInstance->Render_Font(E.szFontTag, E.szText, vPos,
            XMLoadFloat4(&vPremul),
            0.f,
            _float2(0.f, 0.f),
            vRenderScale);
    }
}

ID3D11ShaderResourceView* CUICanvasTool::Get_PreviewSRV(const _tchar* pPath)
{
    if (nullptr == pPath || 0 == pPath[0])
        return nullptr;

    _wstring strKey = pPath;
    auto it = m_PreviewCache.find(strKey);
    if (m_PreviewCache.end() != it)
        return (nullptr != it->second) ? it->second->Get_SRV(0) : nullptr;

    CTexture* pTex = CTexture::Create(m_pDevice, m_pContext, pPath, 1);
    m_PreviewCache[strKey] = pTex;
    return (nullptr != pTex) ? pTex->Get_SRV(0) : nullptr;
}

_int CUICanvasTool::Add_Element(UI_ELEMENT_TYPE eType)
{
    UI_ELEMENT Element = {};
    Init_Default(&Element, eType);
    m_SceneData.Elements.push_back(Element);
    Reindex_ZOrders();
    m_iSelectedIndex = static_cast<_int>(m_SceneData.Elements.size()) - 1;
    return m_iSelectedIndex;
}

void CUICanvasTool::Init_Default(UI_ELEMENT* pElement, UI_ELEMENT_TYPE eType)
{
    ZeroMemory(pElement, sizeof(UI_ELEMENT));
    pElement->eType = eType;
    pElement->fCenterX = 640.f;
    pElement->fCenterY = 360.f;
    pElement->fSizeX = 200.f;
    pElement->fSizeY = 200.f;
    pElement->iZOrder = static_cast<_uint>(m_SceneData.Elements.size());
    pElement->bVisible = true;
    pElement->vColor = _float4{ 1.f, 1.f, 1.f, 1.f };
    pElement->iAtlasCols = 1;
    pElement->iAtlasRows = 1;
    pElement->fFrameDuration = 0.1f;
    pElement->bLoop = true;
    pElement->fPlaybackSpeed = 1.f;
    pElement->eHAlign = UI_TEXT_HALIGN::CENTER;
    pElement->eVAlign = UI_TEXT_VALIGN::MIDDLE;
    pElement->eSweepMode = UI_SWEEP_MODE::NONE;

    _tchar szName[MAX_PATH] = {};
    const _tchar* pPrefix = TEXT("Element");
    switch (eType)
    {
    case UI_ELEMENT_TYPE::IMAGE:            
        pPrefix = TEXT("Image");                
        break;
    case UI_ELEMENT_TYPE::TEXT:                     
        pPrefix = TEXT("Text");                 
        break;
    case UI_ELEMENT_TYPE::SPRITE_ANIM:      
        pPrefix = TEXT("SpriteAnim");   
        break;
    case UI_ELEMENT_TYPE::VIDEO:            
        pPrefix = TEXT("Video");                
        break;
    default: 
        break;
    }
    swprintf_s(szName, TEXT("%s_%u"), pPrefix, m_iNameCounter++);
    wcscpy_s(pElement->szName, szName);

    if (UI_ELEMENT_TYPE::TEXT == eType)
    {
        wcscpy_s(pElement->szFontTag, TEXT("Font_Default"));
        wcscpy_s(pElement->szText, TEXT("Text"));
    }
    if (UI_ELEMENT_TYPE::SPRITE_ANIM == eType)
    {
        pElement->iAtlasCols = 4;
        pElement->iAtlasRows = 4;
        pElement->fFrameDuration = 0.083f;
    }
    if (UI_ELEMENT_TYPE::VIDEO == eType)
    {
        // 영상은 풀스크린 기본 (1280x720 캔버스 중앙)
        pElement->fSizeX = 1280.f;
        pElement->fSizeY = 720.f;
    }
}

ImVec2 CUICanvasTool::Canvas_To_Screen(_float cx, _float cy, const ImVec2& vImagePos, _uint iViewportW, _uint iViewportH) const
{
    const _float fScaleX = static_cast<_float>(iViewportW) / m_SceneData.fAuthoringWidth;
    const _float fScaleY = static_cast<_float>(iViewportH) / m_SceneData.fAuthoringHeight;
    return ImVec2(vImagePos.x + cx * fScaleX, vImagePos.y + cy * fScaleY);
}

_int CUICanvasTool::Pick_Element(const ImVec2& vMouse, const ImVec2& vImagePos, _uint iViewportW, _uint iViewportH, DRAG_MODE* pOutMode) const
{
    if (nullptr != pOutMode) *pOutMode = DRAG_MODE::NONE;

    const _float fHandleHalf = 6.f;

    // 1) 선택된 엘리먼트 핸들 우선
    if (m_iSelectedIndex >= 0 && m_iSelectedIndex < static_cast<_int>(m_SceneData.Elements.size()))
    {
        const UI_ELEMENT& E = m_SceneData.Elements[m_iSelectedIndex];
        const _float fHalfX = E.fSizeX * 0.5f;
        const _float fHalfY = E.fSizeY * 0.5f;
        const ImVec2 vTL = Canvas_To_Screen(E.fCenterX - fHalfX, E.fCenterY - fHalfY, vImagePos, iViewportW, iViewportH);
        const ImVec2 vTR = Canvas_To_Screen(E.fCenterX + fHalfX, E.fCenterY - fHalfY, vImagePos, iViewportW, iViewportH);
        const ImVec2 vBL = Canvas_To_Screen(E.fCenterX - fHalfX, E.fCenterY + fHalfY, vImagePos, iViewportW, iViewportH);
        const ImVec2 vBR = Canvas_To_Screen(E.fCenterX + fHalfX, E.fCenterY + fHalfY, vImagePos, iViewportW, iViewportH);

        auto HitHandle = [&](const ImVec2& p) -> _bool
            {
                return fabsf(vMouse.x - p.x) <= fHandleHalf && fabsf(vMouse.y - p.y) <= fHandleHalf;
            };

        if (HitHandle(vTL)) { if (pOutMode) *pOutMode = DRAG_MODE::RESIZE_TL; return m_iSelectedIndex; }
        if (HitHandle(vTR)) { if (pOutMode) *pOutMode = DRAG_MODE::RESIZE_TR; return m_iSelectedIndex; }
        if (HitHandle(vBL)) { if (pOutMode) *pOutMode = DRAG_MODE::RESIZE_BL; return m_iSelectedIndex; }
        if (HitHandle(vBR)) { if (pOutMode) *pOutMode = DRAG_MODE::RESIZE_BR; return m_iSelectedIndex; }
    }

    // 2) 본체 hit test — 역순 (top z first)
    for (_int i = static_cast<_int>(m_SceneData.Elements.size()) - 1; i >= 0; --i)
    {
        const UI_ELEMENT& E = m_SceneData.Elements[i];
        const _float fHalfX = E.fSizeX * 0.5f;
        const _float fHalfY = E.fSizeY * 0.5f;
        const ImVec2 vTL = Canvas_To_Screen(E.fCenterX - fHalfX, E.fCenterY - fHalfY, vImagePos, iViewportW, iViewportH);
        const ImVec2 vBR = Canvas_To_Screen(E.fCenterX + fHalfX, E.fCenterY + fHalfY, vImagePos, iViewportW, iViewportH);

        if (vMouse.x >= vTL.x && vMouse.x <= vBR.x &&
            vMouse.y >= vTL.y && vMouse.y <= vBR.y)
        {
            if (pOutMode) *pOutMode = DRAG_MODE::MOVE;
            return i;
        }
    }

    return -1;
}

void CUICanvasTool::Apply_Drag(const ImVec2& vMouseDelta, _uint iViewportW, _uint iViewportH)
{
    UI_ELEMENT* pE = Get_SelectedElement();
    if (nullptr == pE) return;

    const _float fScaleX = static_cast<_float>(iViewportW) / m_SceneData.fAuthoringWidth;
    const _float fScaleY = static_cast<_float>(iViewportH) / m_SceneData.fAuthoringHeight;
    const _float fDx = vMouseDelta.x / fScaleX;
    const _float fDy = vMouseDelta.y / fScaleY;

    switch (m_eDragMode)
    {
    case DRAG_MODE::MOVE:
        pE->fCenterX = m_fStartCX + fDx;
        pE->fCenterY = m_fStartCY + fDy;
        break;
    case DRAG_MODE::RESIZE_TL:
        pE->fCenterX = m_fStartCX + fDx * 0.5f;
        pE->fCenterY = m_fStartCY + fDy * 0.5f;
        pE->fSizeX = max(1.f, m_fStartSX - fDx);
        pE->fSizeY = max(1.f, m_fStartSY - fDy);
        break;
    case DRAG_MODE::RESIZE_TR:
        pE->fCenterX = m_fStartCX + fDx * 0.5f;
        pE->fCenterY = m_fStartCY + fDy * 0.5f;
        pE->fSizeX = max(1.f, m_fStartSX + fDx);
        pE->fSizeY = max(1.f, m_fStartSY - fDy);
        break;
    case DRAG_MODE::RESIZE_BL:
        pE->fCenterX = m_fStartCX + fDx * 0.5f;
        pE->fCenterY = m_fStartCY + fDy * 0.5f;
        pE->fSizeX = max(1.f, m_fStartSX - fDx);
        pE->fSizeY = max(1.f, m_fStartSY + fDy);
        break;
    case DRAG_MODE::RESIZE_BR:
        pE->fCenterX = m_fStartCX + fDx * 0.5f;
        pE->fCenterY = m_fStartCY + fDy * 0.5f;
        pE->fSizeX = max(1.f, m_fStartSX + fDx);
        pE->fSizeY = max(1.f, m_fStartSY + fDy);
        break;
    default:
        break;
    }
}

void CUICanvasTool::Reindex_ZOrders()
{
    for (size_t i = 0; i < m_SceneData.Elements.size(); ++i)
        m_SceneData.Elements[i].iZOrder = static_cast<_uint>(i);
}

CUICanvasTool* CUICanvasTool::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    CUICanvasTool* pInstance = new CUICanvasTool(pDevice, pContext);
    wcscpy_s(pInstance->m_SceneData.szName, TEXT("UIScene"));
    pInstance->m_SceneData.fAuthoringWidth = 1280.f;
    pInstance->m_SceneData.fAuthoringHeight = 720.f;
    return pInstance;
}

void CUICanvasTool::Free()
{
    __super::Free();

    for (auto& Pair : m_PreviewCache)
        Safe_Release(Pair.second);
    m_PreviewCache.clear();

    Safe_Release(m_pDevice);
    Safe_Release(m_pContext);
}