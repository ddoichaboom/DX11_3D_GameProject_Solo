#include "Panel_Inspector.h"
#include "Panel_Manager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Transform.h"
#include "Component.h"
#include "Model.h"


CPanel_Inspector::CPanel_Inspector(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: CPanel{ pDevice, pContext }
{
}

HRESULT CPanel_Inspector::Initialize()
{
	strcpy_s(m_szName, "Inspector");

	return S_OK;
}

void CPanel_Inspector::Update(_float fTimeDelta)
{
}

void CPanel_Inspector::Render()
{
	ImGui::Begin(m_szName, &m_bOpen);

	CGameObject* pSelected = m_pPanel_Manager->Get_SelectedObject();
	if (nullptr == pSelected)
	{
		ImGui::TextDisabled("No Object Selected");
		ImGui::End();
		return;
	}

	Render_GameObject(pSelected);

	CTransform* pTransform = pSelected->Get_Transform();
	if (nullptr != pTransform)
		Render_Transform(pTransform);

	Render_Model(pSelected);

	ImGui::End();
}

void CPanel_Inspector::Render_GameObject(CGameObject* pObject)
{
	if (!ImGui::CollapsingHeader("GameObject", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	rttr::type type = rttr::type::get<CGameObject>();

	for (auto& prop : type.get_properties())
	{
		Render_Property(prop, *pObject);
	}
}

void CPanel_Inspector::Render_Transform(CTransform* pTransform)
{
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	_bool bGizmoUsing = ImGuizmo::IsUsing();

	if (bGizmoUsing)
		ImGui::BeginDisabled();

	rttr::type type = rttr::type::get<CTransform>();

	for (auto& prop : type.get_properties())
	{
		Render_Property(prop, *pTransform);
	}

	if (bGizmoUsing)
		ImGui::EndDisabled();
}

void CPanel_Inspector::Render_Property(rttr::property prop, rttr::instance instance)
{
	std::string strName = prop.get_name().to_string();
	rttr::variant value = prop.get_value(instance);

	if (!value.is_valid())
		return;

	// _float3 -> DragFloat3
	if (value.is_type<_float3>())
	{
		_float3 v = value.get_value<_float3>();

		if (ImGui::DragFloat3(strName.c_str(), &v.x, 0.1f))
		{
			prop.set_value(instance, v);
		}
	}
	// _float -> DragFloat
	else if (value.is_type<_float>())
	{
		_float f = value.get_value<_float>();

		if (ImGui::DragFloat(strName.c_str(), &f, 0.1f))
		{
			prop.set_value(instance, f);
		}
	}
	// _bool -> CheckBox
	else if (value.is_type<_bool>())
	{
		_bool b = value.get_value<_bool>();

		if (ImGui::Checkbox(strName.c_str(), &b))
		{
			prop.set_value(instance, b);
		}
	}
	// _int -> DragInt
	else if (value.is_type<_int>())
	{
		_int i = value.get_value<_int>();

		if (ImGui::DragInt(strName.c_str(), &i))
		{
			prop.set_value(instance, i);
		}
	}
	// wstring -> InputText
	else if (value.is_type<_wstring>())
	{
		_wstring wstr = value.get_value<_wstring>();
		std::string str = WTOA(wstr);

		char szBuff[256] = {};
		strcpy_s(szBuff, str.c_str());

		if (ImGui::InputText(strName.c_str(), szBuff, sizeof(szBuff),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			prop.set_value(instance, ATOW(std::string(szBuff)));
		}
	}
	// 미지원 타입 (필요시 확장)
	else
	{
		ImGui::TextDisabled("%s: (Unsupported Type)", strName.c_str());
	}
}

void CPanel_Inspector::Render_Model(CGameObject* pObject)
{
	auto& Components = pObject->Get_Components();
	auto iter = Components.find(TEXT("Com_Model"));
	if (iter == Components.end())
		return;

	CModel* pModel = static_cast<CModel*>(iter->second);
	if (nullptr == pModel)
		return;

	if (!ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	_bool bIsAnim = pModel->Get_ModelType() == MODEL::ANIM;
	const char* szType = bIsAnim ? "ANIM" : "NONANIM";

	ImGui::Text("Type: %s", szType);
	ImGui::Text("Meshes: %d", pModel->Get_NumMeshes());
	ImGui::Text("Materials: %d", pModel->Get_NumMaterials());

	{
		char szRootBone[MAX_PATH] = {};
		strcpy_s(szRootBone, pModel->Get_RootBoneName());
		if (ImGui::InputText("Root Bone", szRootBone, MAX_PATH, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			pModel->Set_RootBoneName(szRootBone);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Enter to apply)");

		if (ImGui::Button("Save Model (.bin)"))
		{
			HRESULT hr = pModel->Save_Binary();
			if (SUCCEEDED(hr))
				Log_Info("Model saved (.bin v2). .bak backup created.");
			else
				Log_Error("Save Model failed (no binary path or write error).");
		}
	}
	ImGui::Separator();

	// Bone List
	_uint iNumBones = pModel->Get_NumBones();

	ImGui::Text("Bones: %d", iNumBones);
	if (iNumBones > 0 && ImGui::TreeNode("Bone List"))
	{
		for (size_t i = 0; i < iNumBones; i++)
		{
			const _char* szBoneName = pModel->Get_BoneName(i);
			_int iParentIndex = pModel->Get_BoneParentIndex(i);

			char szLabel[MAX_PATH] = {};
			sprintf_s(szLabel, "[%d] %s (Parent : %d)", i, szBoneName, iParentIndex);

			if (-1 == iParentIndex)
			{
				// Root : 강조 표시
				ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "%s", szLabel);
			}
			else
			{
				ImGui::TextUnformatted(szLabel);
			}
		}
		ImGui::TreePop();
	}

	if (!bIsAnim)
		return;

	ImGui::Text("Animations: %d", pModel->Get_NumAnimations());
	ImGui::Separator();

	_bool bPlaying = pModel->Get_AnimationPlaying();
	if (ImGui::Checkbox("Playing", &bPlaying))
	{
		pModel->Set_AnimationPlaying(bPlaying);
	}

	_bool bLoop = pModel->Get_AnimationLoop();
	if (ImGui::Checkbox("Loop", &bLoop))
	{
		pModel->Set_AnimationLoop(bLoop);
	}
	
	// Root Motion
	_bool bRootMotion = pModel->Get_RootMotionEnabled();
	if (ImGui::Checkbox("Root Motion", &bRootMotion))
	{
		pModel->Set_RootMotionEnabled(bRootMotion);
	}

	_float3 vDelta = pModel->Get_LastRootMotionDelta();
	ImGui::Text("Root Delta: %.4f, %.4f, %.4f", vDelta.x, vDelta.y, vDelta.z);

	_float fTrackPos = pModel->Get_TrackPosition();
	_float fDuration = pModel->Get_Duration();
	_float fProgress = (fDuration > 0.f) ? (fTrackPos / fDuration) : 0.f;

	char szOverlay[64] = {};
	sprintf_s(szOverlay, "%.1f / %.1f", fTrackPos, fDuration);
	ImGui::ProgressBar(fProgress, ImVec2(-1.f, 0.f), szOverlay);

	// 애니메이션 리스트
	_uint iCurrentIndex = pModel->Get_CurrentAnimIndex();
	_uint iNumAnims = pModel->Get_NumAnimations();

	if (ImGui::TreeNode("Animation List"))
	{
		ImGui::TextDisabled("Loop RM [Idx] Name");

		ImGui::BeginChild(
			"##AnimListScroll",
			ImVec2(0.f, 360.f),
			true,
			ImGuiWindowFlags_HorizontalScrollbar);

		for (size_t i = 0; i < iNumAnims; i++)
		{
			ImGui::PushID(static_cast<int>(i));

			// Loop per-anim
			_bool bLoopI = pModel->Get_AnimationLoop(static_cast<_uint>(i));
			if (ImGui::Checkbox("##loop", &bLoopI))
				pModel->Set_AnimationLoop(static_cast<_uint>(i), bLoopI);

			ImGui::SameLine();

			// UseRootMotion per-anim
			_bool bRMI = pModel->Get_AnimationUseRootMotion(static_cast<_uint>(i));
			if (ImGui::Checkbox("##rm", &bRMI))
				pModel->Set_AnimationUseRootMotion(static_cast<_uint>(i), bRMI);

			ImGui::SameLine();

			char szLabel[MAX_PATH] = {};
			sprintf_s(szLabel, "[%d] %s",
				static_cast<int>(i),
				pModel->Get_AnimationName(static_cast<_uint>(i)));

			_bool bSelected = (i == iCurrentIndex);
			if (ImGui::Selectable(szLabel, bSelected))
			{
				if (!bSelected)
					pModel->Set_AnimationIndex(static_cast<_uint>(i));
				else
					pModel->Restart_Animation();
			}

			// C-5: 선택된 애니메이션에만 Notify 편집 UI 표시
			if (bSelected)
			{
				ImGui::Indent();

				const _uint iAnim = static_cast<_uint>(i);
				const _uint iNumNotifies = pModel->Get_NumNotifies(iAnim);
				const _float fAnimDuration = pModel->Get_Duration();
				const _float fTPS = pModel->Get_TickPerSecond();

				ImGui::Text("Notifies (%u)", iNumNotifies);

				// ANIM_NOTIFY_TYPE enum 순서와 반드시 일치
				static const char* s_TypeNames[] = {
					"NONE",
					"FOOTSTEP_L",
					"FOOTSTEP_R",
					"ATTACK_HIT",
					"COMBO_WINDOW_OPEN",
					"COMBO_WINDOW_CLOSE",
					"ATTACK_HITBOX_ON",
					"ATTACK_HITBOX_OFF",
					"DETECT_ON",
					"DETECT_OFF",
					"INVINCIBLE_ON",
					"INVINCIBLE_OFF",
				};

				_int iRemoveIndex = -1;     // 삭제 요청 캐시 (루프 종료 후 1회 처리)
				for (_uint k = 0; k < iNumNotifies; ++k)
				{
					ImGui::PushID(static_cast<int>(k + 10000));     // outer i 와 충돌 회피

					ANIM_NOTIFY N = pModel->Get_Notify(iAnim, k);

					// [tick 드래그]
					ImGui::SetNextItemWidth(120.f);
					_float fTick = N.fTick;
					if (ImGui::DragFloat("##tick", &fTick, 0.1f, 0.f, fAnimDuration, "Tick %.2f"))
						pModel->Set_NotifyTick(iAnim, k, fTick);

					// 드래그 종료 시점에만 정렬 (도중 정렬 시 ID 충돌)
					if (ImGui::IsItemDeactivatedAfterEdit())
						pModel->Sort_Notifies(iAnim);

					// [초 환산 표시]
					ImGui::SameLine();
					ImGui::Text("@%.2fs", (fTPS > 0.f) ? (fTick / fTPS) : 0.f);

					// [타입 dropdown]
					ImGui::SameLine();
					ImGui::SetNextItemWidth(180.f);
					_int iType = static_cast<_int>(N.eType);
					if (ImGui::Combo("##type", &iType, s_TypeNames, IM_ARRAYSIZE(s_TypeNames)))
						pModel->Set_NotifyType(iAnim, k, static_cast<ANIM_NOTIFY_TYPE>(iType));

					// [삭제 버튼]
					ImGui::SameLine();
					if (ImGui::Button("X"))
						iRemoveIndex = static_cast<_int>(k);

					ImGui::PopID();
				}

				if (iRemoveIndex >= 0)
					pModel->Remove_Notify(iAnim, static_cast<_uint>(iRemoveIndex));

				if (ImGui::SmallButton("+ Add Notify"))
				{
					ANIM_NOTIFY New{};
					New.fTick = 0.f;        // 0 tick 으로 추가, 사용자가 drag 로 이동
					New.eType = ANIM_NOTIFY_TYPE::NONE;
					pModel->Add_Notify(iAnim, New);
				}

				ImGui::Unindent();
			}

			ImGui::PopID();         // *** 위치 이동: Notify UI 까지 감싸도록 ***
		}
		ImGui::EndChild();

		ImGui::TreePop();
	}

	ImGui::Separator();
	if (ImGui::Button("Save Model Binary"))
	{
		pModel->Save_Binary();
	}
}

CPanel_Inspector* CPanel_Inspector::Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	CPanel_Inspector* pInstance = new CPanel_Inspector(pDevice, pContext);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Create : CPanel_Inspector");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CPanel_Inspector::Free()
{
	__super::Free();
}
