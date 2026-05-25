# R8 #85 — HUD 확장 세부 계획 (스킬 슬롯 / 퀘스트 / 콤보 / 데미지 폰트)

> 작성일: 2026-05-25
> 상위: `명세서/통합_구현계획_v3.md` §6 (트랙 5 잔여 Player) R8
> 선행 SSOT: `명세서/5월 24일 진행 사항.md` §1.7 ("다음 = R7 #84 → R8 #85 → 이펙트 트랙")
> 본 문서는 R8 진입 시점 **설계/결정 SSOT**. 구현 진행에 따라 "구현 로그" 절을 갱신한다.

---

## 0. 범위 (사용자 확정 2026-05-25)

실제 인게임 스크린샷(`스크린샷 2026-05-25 122710/122721.png`) 기반으로 R8 을 4 덩어리로 확장한다.

| 덩어리   | 내용                                                                                         | 위치                    |
| -------- | -------------------------------------------------------------------------------------------- | ----------------------- |
| **R8-A** | 스킬/키 매핑 5슬롯 HUD (C/F/Q/E/R) — Base + 무기별 아이콘 교체 + Cool UV sweep 쿨다운        | 우측 하단               |
| **R8-B** | 퀘스트 목표 텍스트 (알람 아이콘 + 텍스트)                                                    | 우측 상단               |
| **R8-C** | 콤보 시스템 — 타격수 누적/감소 → 등급(D~SSS) + 타격수 숫자                                   | 우측 상단 (퀘스트 아래) |
| **R8-D** | 피격 데미지 폰트 — 피격 GameObject Transform 중심 Sphere 외곽 랜덤 위치, RectInstance 재사용 | 월드(빌보드)            |

**진행 순서 (확정)**: **A → B → C → D**. 근거: A/B 는 기존 `CHUD_GamePlay` + `HUD.uiscene` + `CUI_Image/CUI_Text` 인프라 직결로 회귀 위험 최소. C 는 신규 콤보 로직 + 폰트, D 는 신규 빌보드 배치 로직이라 뒤로.

---

## 1. 준비된 자산 (사용자 제공, 직접 매핑 예정)

### 1.1 `Resources/Textures/HUD/`

**스킬 슬롯 프레임/상태**

- `Control_Skill_Default_Base` — 일반 슬롯 베이스
- `Control_Skill_Default_Active` / `_Press` — 활성/입력 피드백
- `Control_Skill_Default_Cool` — **쿨다운 오버레이 (UV sweep 대상)**
- `Control_Skill_UT_Base` / `Control_Skill_Ultimate_Active` — 궁극기(R) 슬롯
- `Frame_Hud_Skill_QTE_Active` — QTE(Left Shift) 활성 프레임
- `Frame_Hud_Set1_Base` / `_Outline`, `Frame_Hud_Deco_Glow` / `_Outline` — HUD 공통 장식
- `Skill_U_CRank` — 궁극기(R) 아이콘 (확정) / `Skill_iCon` (미사용 백업)

**스킬 아이콘 (무기/키별)**

- C 스왑: `Icon_Weapon_KasakaVenomFang` ↔ `Icon_Weapon_KnightKiller` (장착 무기 토글)
- F 고유: Kasaka `GS_Skill01_SSR_KasakaVenomFang` / KnightKiller `GS_Skill01_KnightKiller`
- Q: `Skill_03_02`
- E: Kasaka `Skill_08` / KnightKiller `Skill_06`

**퀘스트**

- `Icon_BattleMission_Hud_Alarm` — 우측 상단 미션 알람 아이콘

**콤보 등급 (개별, 미사용 — 아틀라스 채택)**

- `Text_Rank_D/C/B/A/S/SS/SSS` → **R8-C 는 아틀라스 채택으로 미사용** (백업 자산)

**기존(사용 중)**: HP/MP/Monster/Dash, `back_light_line`, `HP_BarLight` 등

### 1.2 `Resources/Textures/Font/`

- `Atlas_DamageFont_BG_Black` — 0~9, **검은 배경**. RectInstance 셰이더에 적합 (§2)
- `Atlas_DamageFont_BG_NONE` — 0~9, 투명 배경+외곽선. RectInstance 부적합 (알파블렌드 UI 전용 백업)
- `Atlas_ComboFont` — D·C·B·A·S·SS·SSS **7프레임 가로 아틀라스** (등급별 색상 내장)

### 1.3 부족 시 보충 경로

`C:\SL\FullExtract_Texture2D`, `C:\SL\Filltered_MonoBehaviour\HUD`

---

## 2. 데미지/콤보 폰트 discard 결정 (확정)

`Resources/ShaderFiles/Shader_VtxRectInstance.hlsl` PS (62~69):

```hlsl
float fMaxRGB = max(max(vTexColor.r, vTexColor.g), vTexColor.b);
if (fMaxRGB <= 0.035f) discard;          // 검정(rgb≈0) 키 컬러 discard. 알파 미사용
Out.vColor = float4(vTexColor.rgb, 1.f) * In.vColor;   // 알파 1 강제, RGB 밝기로 표현
Out.vColor.a *= g_fAlpha;
```

BlendState = `BS_AlphaBlend`, Cull None, DSS_Default.

**결정**:

- **데미지 폰트 = `Atlas_DamageFont_BG_Black`** 를 RectInstance 그대로 사용. 검은 배경 자동 discard, 회색 숫자만 표시. **셰이더 수정 0**.
- `BG_NONE` 은 이 셰이더와 불일치(알파 미참조 → 투명 배경 안 사라짐 + 속 빈 외곽선) → RectInstance 미사용.
- **콤보 타격수 숫자도 동일 BG_Black 숫자 아틀라스 재사용** (사용자 확정). 콤보 등급은 `Atlas_ComboFont` 아틀라스 (사용자 확정).
- 등급/숫자 색을 흰 기본에서 바꾸려면 인스턴스 `vColor` 곱으로 조정(셰이더가 `* In.vColor`).

---

## 3. R8-A — 스킬 5슬롯 HUD (우측 하단)

### 3.1 슬롯 모델

각 슬롯(C/F/Q/E/R)은 레이어드 UI_Image 묶음:

```
[Base 프레임] → [스킬 아이콘(무기별 교체)] → [Cool 오버레이(세로 차오름, 쿨다운 중만)]
```

- **Base**: C/F/Q/E `Control_Skill_Default_Base`, R `Control_Skill_UT_Base` (정적)
- **Icon**: 장착 무기(`Get_EquippedWeapon()`)에 따라 F/E/C 텍스처 교체(Q 고정). 교체 방식 = §3.3 옵션 B
- **Cool**: `Control_Skill_Default_Cool`. **세로(하단→상단) 차오름 게이지** (사용자 결정 2026-05-25).
  쿨다운 잔여>0 동안 Visible + `Set_GaugeRatio(1 - 잔여/최대)` → 아래에서 위로 차오름. 가득 차는 순간(쿨 완료, 잔여≤0) Visible=false.
  `UI_SWEEP_MODE=NONE`. **세로 게이지는 PS_UI 신규 분기(`g_fGaugeVertical`) 필요** (§3.6).

### 3.2 데이터 소스 (Player API, 기존)

- `SKILL_SLOT { Q, E, F, R }` (count 4), `SKILL_COOLDOWN[]={5,5,5,8}`
- `Get_SkillCooldownTimer(SKILL_SLOT)`, `Get_SkillCooldownMax(SKILL_SLOT)`, `Can_UseSkill(SKILL_SLOT)`
- C(WeaponSwap) 쿨다운: 별도 (`WEAPON_SWAP_COOLDOWN=1.5f`) — Player 에 `Get_WeaponSwapCooldownTimer/Max` 게터 추가 필요
- `Get_EquippedWeapon() : EQUIPPED_WEAPON_ID` (NONE / KASAKA_VENOM_FANG / KNIGHT_KILLER)

### 3.3 무기별 아이콘 교체 = 옵션 B (런타임 텍스처 프로토타입 교체) — 확정 2026-05-25

**근거**: 이 프로젝트는 HUD 텍스처를 이미 **프로토타입 등록 후 Clone 공유** 방식으로 운용(아래). 프로토타입을 쓰면 슬롯당 ICON UI 1개만 두고 런타임에 텍스처 컴포넌트만 교체하는 게 가장 깔끔(객체 수 최소, 교체 비용 = SRV AddRef).

**기존 인프라 (이미 존재)**:

- `CUI_Image::Ready_Components` 는 `pTextureProtoTag` 가 있으면 **Pattern A(프로토타입 Clone)**, 없으면 path 동적 로드(Editor 미리보기 폴백). 즉 프로토타입 우선.
- `Loader.cpp` 에 기존 HUD 텍스처가 전부 `Prototype_Component_Texture_HUD_*` 로 **GAMEPLAY 레벨** 등록됨 (Loader.cpp:134 `eLevel = LEVEL::GAMEPLAY`). ※ STATIC 아님 — Clone/조회 레벨도 GAMEPLAY 일치 필요.

**구현**:

- `CUI_Image` 에 `Set_TextureByProto(_uint iLevel, const _tchar* pProtoTag)` 신설 — `GameInstance->Clone_Prototype(PROTOTYPE::COMPONENT, iLevel, pProtoTag)` → 기존 `m_pTextureCom` Release 후 교체. **Engine 무영향** (Client 클래스 메서드).
- `CHUD_GamePlay::Tick_Skills` 에서 `Get_EquippedWeapon()` 변화 감지 시 F/E/C ICON 슬롯에 무기별 protoTag 교체.

### 3.4 HUD_SLOT enum 확장

`Client_Enum.h::HUD_SLOT` 에 스킬 슬롯 추가 (END 앞). Cache_UIs `Names[]` 배열과 1:1. ICON 은 슬롯당 **단일**(옵션 B 로 런타임 교체하므로 무기별 분리 불필요). 이름 규약:

```
SKILL_C_BASE, SKILL_C_ICON, SKILL_C_COOL,
SKILL_F_BASE, SKILL_F_ICON, SKILL_F_COOL,
SKILL_Q_BASE, SKILL_Q_ICON, SKILL_Q_COOL,
SKILL_E_BASE, SKILL_E_ICON, SKILL_E_COOL,
SKILL_R_BASE, SKILL_R_ICON, SKILL_R_COOL,
```

### 3.5 신규 텍스처 프로토타입 (Loader.cpp 확장 — 기존 패턴)

`Prototype_Component_Texture_HUD_*` 패턴으로 **GAMEPLAY** 등록(Loader eLevel). 최소:

```
Skill_Base / Skill_Cool / Skill_UT_Base / Skill_Ultimate         (프레임)
Icon_Weapon_Kasaka / Icon_Weapon_KnightKiller                    (C 슬롯)
Skill_F_Kasaka(GS_..KasakaVenomFang) / Skill_F_KK(GS_..KnightKiller)
Skill_Q(Skill_03_02)
Skill_E_Kasaka(Skill_08) / Skill_E_KK(Skill_06)
```

uiscene 의 슬롯 UI 는 `pTextureProtoTag` 로 위 프로토타입 참조(초기값 = 기본 장착 무기 KASAKA 기준). ICON 은 런타임 `Set_TextureByProto` 로 교체.

### 3.6 셰이더 세로 게이지 (Shader_VtxTex.hlsl `PS_UI`)

기존 `PS_UI` 는 가로(x) 게이지(`if vTexcoord.x > g_fGaugeProgress discard`) — HP/MP Fill 이 사용 중이라 **유지**. 방향 uniform 추가:

```hlsl
float g_fGaugeVertical = 0.f;   // 0=가로(L→R, 기존) / 1=세로(하단→상단 차오름)
...
// PS_UI 내 게이지 분기
if (g_fGaugeVertical < 0.5f) { if (In.vTexcoord.x > g_fGaugeProgress) discard; }
else                         { if (In.vTexcoord.y < 1.f - g_fGaugeProgress) discard; }
```

- 기본 0 → 기존 UI 무영향 (회귀 0). `CUI_Image` 에 `m_fGaugeVertical` + `Set_GaugeVertical(bool)` + Bind_RawValue 추가.
- Cool 오버레이만 `Set_GaugeVertical(true)`.
- ⚠️ **함정 (2026-05-25)**: `PS_UI` 는 UIPass(`CUI_Image`/`CUI_Video` Begin(1)) + SpriteAnimPass(`CUI_SpriteAnim` Begin(2)) 가 **공유**. Effects11 은 HLSL 초기값(`g_fGaugeVertical=0.f`)을 **보장 안 함** → PS_UI 를 쓰는 모든 UI 가 `g_fGaugeProgress`/`g_fGaugeVertical` 을 바인딩해야 함. 미바인딩 시 garbage → `lerp` 이상치 → `discard` (타이틀 Video 사라짐 버그). `CUI_Video`/`CUI_SpriteAnim` Bind 에 `g_fGaugeProgress=1, g_fGaugeVertical=0` 추가로 해결.

### 3.7 uiscene 저작 (협업 분담)

`HUD.uiscene` 은 바이너리(`SLUI`) → **Editor `CPanel_2DCanvas` 로 사용자가 저작**. 코드(§3.4 이름 규약 + §3.5 protoTag)를 먼저 확정 → 사용자가 Editor 에서 슬롯 UI 를 그 이름/protoTag/위치로 배치·저장. Cool 슬롯은 Editor 에서 세로게이지 플래그 지정(또는 코드 Cache 시 강제 `Set_GaugeVertical(true)`).

---

## 4. R8-B — 퀘스트 목표 (우측 상단)

- `Icon_BattleMission_Hud_Alarm` (UI_Image) + 목표 텍스트 1~2줄 (`CUI_Text`, 기존 SpriteFont)
- 데이터: 현재 퀘스트 시스템 없음 → **정적 텍스트 표시만** (사용자 요청: "퀘스트 목표까지만 띄우면 돼"). 추후 퀘스트 시스템 도입 시 바인딩.

---

## 5. R8-C — 콤보 시스템 (우측 상단, 퀘스트 아래)

### 5.1 로직 (소유: CHUD_GamePlay 또는 신규 CCombo_Manager)

- 타격 발생 시 `m_iComboCount++`, `m_fComboDecayTimer = COMBO_DECAY`(예 3.0s) 리셋
- 매 프레임 `m_fComboDecayTimer -= dt`; 0 도달 시 콤보 **감소/리셋**
- 등급 산정: 타격수 구간 → D/C/B/A/S/SS/SSS (임계값 튜닝, 예: 0~4 D / 5~9 C / 10~19 B / 20~34 A / 35~54 S / 55~79 SS / 80+ SSS)
- 트리거 진입점: `Notify_Hit`(몬스터 피격) 또는 플레이어 공격 적중 콜백. 기존 `Notify_Hit(CMonster*)` 재활용 검토

### 5.2 표현

- 등급: `Atlas_ComboFont` 7프레임 → 등급 인덱스로 UV 프레임 선택
- 타격수: `Atlas_DamageFont` 숫자 → 자릿수별 인스턴스 (RectInstance) 또는 UI 자릿수 배치

---

## 6. R8-D — 데미지 폰트 (월드 빌보드)

### 6.1 요구

- 피격 시 대상 `GameObject` Transform 중심, 반경 `R` Sphere **외곽 정점 중 랜덤 위치**에 데미지 숫자 1회 스폰 → 짧게 떠오르며 fade
- `CAtlasInstanceEffect`(RectInstance) 재사용. `Atlas_DamageFont_BG_Black`

### 6.2 인프라 갭 (R8-D 진입 시 해결)

- 현재 `CAtlasInstanceEffect` 는 **전역 단일 프레임**(`m_iCurFrame`)을 모든 인스턴스 공유 → 다자리 숫자("123") 표현 불가
- 필요: **인스턴스별 atlas frame 지정** (자릿수별 다른 숫자) — Desc/VIBuffer 확장 또는 데미지 전용 분기
- Sphere 외곽 랜덤: `중심 + R * normalize(rand_dir)` (구 표면 균등 샘플) 한 점 → 그 위치에 자릿수 가로 배치

### 6.3 호출처

- `CMonster::Take_Damage` / `CPlayer::Take_Damage` 끝에서 스폰 요청 (기존 `CHUD_GamePlay::Notify_Hit` 경로 인접)

---

## 7. 결정 로그

| 일자       | 결정                                                                                                                                      |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-05-25 | 진행 순서 A→B→C→D                                                                                                                         |
| 2026-05-25 | 콤보 등급 = `Atlas_ComboFont` 아틀라스 (Text*Rank*\* 백업)                                                                                |
| 2026-05-25 | 콤보 타격수 숫자 = `Atlas_DamageFont` 숫자 재사용                                                                                         |
| 2026-05-25 | 데미지 폰트 = `BG_Black` + 기존 RectInstance(검정 키 discard), 셰이더 수정 0                                                              |
| 2026-05-25 | 텍스처 = **프로토타입 등록 + Clone 공유** (사용자 제안). 기존 `Prototype_Component_Texture_HUD_*` 패턴 확장. path 동적 로드는 Editor 폴백 |
| 2026-05-25 | 무기별 아이콘 교체 = **옵션 B 확정** (`CUI_Image::Set_TextureByProto` 런타임 교체). 옵션 A 폐기                                           |
| 2026-05-25 | 쿨다운 표현 = **세로 하단→상단 차오름 게이지**(`Set_GaugeRatio(1-잔여/최대)`), 가득 차면 hide. PS_UI 에 `g_fGaugeVertical` 분기 추가      |
| 2026-05-25 | uiscene 스킬 슬롯 = Editor 2DCanvas 사용자 저작 (코드가 이름규약/protoTag 선확정)                                                         |

---

## 8. 구현 로그

### 2026-05-25 — R8-A 코드 적용 (묶음1~3, 빌드 검증 대기)

Claude 직접 적용(사용자 승인). 파일별 인코딩 보존: Shader_VtxTex.hlsl·UI_Image.cpp·Loader.cpp·HUD_GamePlay.{h,cpp} = CP949(PowerShell `GetEncoding(949)`), Client_Enum.h = UTF-8 BOM(Edit), UI_Image.h = UTF-8 no-BOM.

- **묶음1 (인프라)**:
  - `Shader_VtxTex.hlsl`: `g_fGaugeVertical` uniform 추가. `PS_UI` 게이지를 `lerp(x, 1-y, g_fGaugeVertical)` 통합(0=가로 L→R 기존 / 1=세로 하단→상단). 기본 0 → 기존 HP/MP Fill 무영향.
  - `CUI_Image`: `Set_GaugeVertical(_bool)` / `Is_GaugeVertical()` / `m_bGaugeVertical` + Bind_RawValue("g_fGaugeVertical"). `Set_TextureByProto(iLevel, protoTag)` = `Clone_Prototype(COMPONENT)` → m_pTextureCom 교체(Engine 무영향). `#include "Texture.h"` 추가.
- **묶음2 (enum + Loader)**:
  - `HUD_SLOT` 에 SKILL*{C/F/Q/E/R}*{BASE/ICON/COOL} 15개 추가(END 앞).
  - `Loader.cpp aHUDEntries[]` 에 스킬 텍스처 12개 프로토타입 등록(`Prototype_Component_Texture_HUD_Skill_*`): Base/Cool/UT_Base/Ultimate + Icon_WpnKasaka/WpnKK + Icon_F_Kasaka/F_KK + Icon_Q + Icon_E_Kasaka/E_KK + Icon_R.
- **묶음3 (HUD_GamePlay)**:
  - `Cache_UIs` Names[] 에 15개 객체명(`HUD_Skill_*`) 추가.
  - `Tick_Skills(dt)` 신규 + Update 에서 호출: ① `Get_EquippedWeapon()` 변화 시 `Refresh_SkillIcons`, ② 슬롯별 Cool 오버레이 세로게이지(`Set_GaugeVertical(true)` + `Set_GaugeRatio(1 - 잔여/최대)` + Visible 토글). C=`Get_WeaponSwapCooldown*`, F/Q/E/R=`Get_SkillCooldown*(SKILL_SLOT)`.
  - `Refresh_SkillIcons(eWeapon)`: Kasaka/KnightKiller 분기로 C/F/E ICON 슬롯 `Set_TextureByProto`. Q/R 고정(uiscene 초기값).
  - Cache 블록에서 m_pPlayer Resolve 후 초기 `Refresh_SkillIcons` 1회 + `m_eCachedWeaponId` 캐시.

### 2026-05-25 — R8-A 묶음3-b (CombatInput 연동, 사용자 결정)

스킬 슬롯도 기존 HP/MP 와 동일하게 **CombatInput 시에만 표시**. → uiscene 전부 visible=false.

- `Set_PlayerBars_Visible` eSlots 에 스킬 Base/Icon 10개 추가 → HP/MP 와 함께 ON/OFF(5초).
- `Tick_Skills` Cool 토글 = `m_bCombatInput && 잔여>0` (전투HUD 꺼지면 Cool 도 숨김).
- `Refresh_SkillIcons` / uiscene ProtoLevel **= GAMEPLAY** (Loader eLevel=GAMEPLAY 일치, 초기 STATIC 오기 정정).

### 2026-05-25 — R8-A 묶음3-c (키 라벨 배경 박스 = 절차 셰이더, 사용자 결정 B)
키 라벨(C/F/Q/E/R)은 SpriteFont 텍스트 + 배경 박스(반투명 회색 + 흰 테두리). 박스는 텍스처 없이 절차 셰이더로.
- `UI_SWEEP_MODE::BOX` 추가 (Client_Enum.h, UV 다음·END 앞 → 기존 값 불변).
- `Shader_VtxTex.hlsl`: `float4 g_vColor` uniform + `PS_BOX`(가장자리 bw=0.12 흰 테두리 / 안쪽 g_vColor 반투명) + `BoxPass`(pass index 5).
- `CUI_Image`: Render `BOX→pass 5`, `Bind_RawValue("g_vColor", &m_vColor)` 무조건 추가(타 PS 무시).
- 사용: 키 박스 = CUI_Image SweepMode=BOX, vColor=(0.3,0.3,0.3,0.5), 텍스처 ProtoTag 아무거나.
- **남은 통합(미적용)**: 키 박스 5개 HUD_SLOT(자동 토글) + 키 글자 5개 CUI_Text(별도 캐싱·토글). 저작 사양도 통합 후 확정.

### 묶음4 — uiscene 저작 가이드 (Editor 2DCanvas, 사용자 작업)

공통: Type=Image / **ProtoLevel=GAMEPLAY** / visible=**OFF**(전부) / SweepMode=NONE.
ProtoTag 풀네임 = `Prototype_Component_Texture_HUD_` + 아래. TexturePath(미리보기, 선택) = `../../Resources/Textures/HUD/` + 파일.

| UI 이름          | ProtoTag             | Texture 파일                       | X    | Y   | Size | z   |
| ---------------- | -------------------- | ---------------------------------- | ---- | --- | ---- | --- |
| HUD_Skill_C_Base | Skill_Base           | Control_Skill_Default_Base.png     | 930  | 660 | 64   | 10  |
| HUD_Skill_C_Icon | Skill_Icon_WpnKasaka | Icon_Weapon_KasakaVenomFang.png    | 930  | 660 | 52   | 11  |
| HUD_Skill_C_Cool | Skill_Cool           | Control_Skill_Default_Cool.png     | 930  | 660 | 64   | 12  |
| HUD_Skill_F_Base | Skill_Base           | Control_Skill_Default_Base.png     | 1005 | 660 | 64   | 10  |
| HUD_Skill_F_Icon | Skill_Icon_F_Kasaka  | GS_Skill01_SSR_KasakaVenomFang.png | 1005 | 660 | 52   | 11  |
| HUD_Skill_F_Cool | Skill_Cool           | Control_Skill_Default_Cool.png     | 1005 | 660 | 64   | 12  |
| HUD_Skill_Q_Base | Skill_Base           | Control_Skill_Default_Base.png     | 1080 | 660 | 64   | 10  |
| HUD_Skill_Q_Icon | Skill_Icon_Q         | Skill_03_02.png                    | 1080 | 660 | 52   | 11  |
| HUD_Skill_Q_Cool | Skill_Cool           | Control_Skill_Default_Cool.png     | 1080 | 660 | 64   | 12  |
| HUD_Skill_E_Base | Skill_Base           | Control_Skill_Default_Base.png     | 1155 | 660 | 64   | 10  |
| HUD_Skill_E_Icon | Skill_Icon_E_Kasaka  | Skill_08.png                       | 1155 | 660 | 52   | 11  |
| HUD_Skill_E_Cool | Skill_Cool           | Control_Skill_Default_Cool.png     | 1155 | 660 | 64   | 12  |
| HUD_Skill_R_Base | Skill_UT_Base        | Control_Skill_UT_Base.png          | 1230 | 660 | 64   | 10  |
| HUD_Skill_R_Icon | Skill_Icon_R         | Skill_U_CRank.png                  | 1230 | 660 | 52   | 11  |
| HUD_Skill_R_Cool | Skill_Cool           | Control_Skill_Default_Cool.png     | 1230 | 660 | 64   | 12  |

- 같은 슬롯 Base/Icon/Cool 은 동일 X·Y 겹침. C·F·E Icon 의 ProtoTag 는 초기값(코드가 무기별 교체), Q·R 고정.
- X·Y 는 시작값(드래그 조정). 저장 후 재빌드 불필요(실행만).

### 2026-05-25 — R8-A 묶음3-d/3-e (키 라벨 + QTE + Active 통합, 재빌드 필요)
- **키 라벨**: `HUD_SLOT::SKILL_{C/F/Q/E/R}_KEYBOX`(5, BOX 모드 CUI_Image, eSlots→CombatInput 자동토글) + 키 글자 `m_pSkillKeyText[6]`(CUI_Text, Cache_UIs 캐싱 `HUD_Skill_*_Key`, Set_PlayerBars_Visible 토글).
- **QTE**: `HUD_SLOT::QTE_FRAME`(Frame_Hud_Skill_QTE_Active) + `QTE_KEYBOX`(BOX) + `m_pSkillKeyText[5]`(="Shift"). 토글 = `Tick_Skills` 의 `Is_QTEWindowActive()`(CombatInput 아님). Loader 에 `QTE_Frame` 등록.
- **Active 글로우(사용가능)**: `HUD_SLOT::SKILL_{C/F/Q/E/R}_ACTIVE`(5). COOL_PAIR 에 `eActive` 추가 → 루프에서 Cool(잔여>0)과 **상호배타**: `pAct->Set_Visible(m_bCombatInput && !(잔여>0 && max>0))`. Loader 에 `Skill_Active`(Default_Active) 등록, R 은 기존 `Skill_Ultimate` 재사용.

### 묶음4-b — uiscene 저작 추가분 (키/QTE/Active, 18개)
공통: visible=**OFF** 전부. 텍스처 있는 것은 ProtoLevel=GAMEPLAY.

| UI 이름 | 타입 | ProtoTag / 텍스트 | SweepMode | vColor | 위치 | z |
|---|---|---|---|---|---|---|
| HUD_Skill_{C/F/Q/E/R}_KeyBox | Image | Skill_Base(BOX가 무시) | BOX | (0.3,0.3,0.3,0.5) | 슬롯 하단 | 10 |
| HUD_Skill_{C/F/Q/E/R}_Key | Text | "C"/"F"/"Q"/"E"/"R" | - | - | 박스 위 | 11 |
| HUD_QTE_Frame | Image | QTE_Frame | NONE | - | 자유(회피 프롬프트) | 10 |
| HUD_QTE_Icon | Image | QTE_Icon | NONE | - | 프레임 중심(겹침) | 11 |
| HUD_QTE_KeyBox | Image | Skill_Base | BOX | (0.3,0.3,0.3,0.5) | QTE 하단 | 10 |
| HUD_QTE_Key | Text | "Shift" | - | - | 박스 위 | 11 |
| HUD_Skill_{C/F/Q/E}_Active | Image | Skill_Active | NONE | - | 슬롯 중심(겹침) | 13 |
| HUD_Skill_R_Active | Image | Skill_Ultimate | NONE | - | R 슬롯 중심 | 13 |

- Active 는 Cool(z=12) 위(z=13). 사용가능 시 Active, 쿨다운 시 Cool — 코드가 상호배타 토글.
- 키 박스/글자, QTE, Active 모두 코드 토글 → uiscene visible 전부 OFF.
- **키박스/QTE박스 SweepMode·vColor 는 코드(Cache)가 강제** (`Set_SweepMode(BOX)` + `Set_Color(0.3,0.3,0.3,0.5)`) — Editor 2DCanvas 에 SweepMode 콤보가 없어서. uiscene 에선 **일반 Image 로 배치만**(ProtoTag=Skill_Base, 위치, visible OFF). SweepMode/vColor 입력 불필요.

> ※ Active 텍스처 3종 전부 Loader 등록 확인: `Skill_Active`(Default_Active, C/F/Q/E), `Skill_Ultimate`(Ultimate_Active, R), `QTE_Frame`(Frame_Hud_Skill_QTE_Active, QTE). QTE 의 ProtoTag 는 자산명이 `..._QTE_Active` 라도 **`QTE_Frame`** 으로 등록됨(혼동 주의).

---

### 2026-05-25 (후반) — 버그 수정 + Cool 세로 재설계 + QTE 연동

**Cool 세로 게이지: PS_UI → PS_GAUGE_V 재설계**
PS_UI 에 `g_fGaugeVertical` lerp 분기를 넣었다가 두 문제 발생: ① 공유 pass(UIPass/SpriteAnimPass)라 Video/SpriteAnim 이 미바인딩 시 garbage→discard. ② PS_UI 가로 롤백 후 `g_fGaugeVertical` 미사용 → Effects11 최적화 제거 → `Bind_RawValue` 가 `IsValid()==false`로 E_FAIL → 전체 UI Bind 실패. → **해결**: PS_UI 가로 원복 + **`PS_GAUGE_V` 전용 pass(pass 7) + `UI_SWEEP_MODE::GAUGE_V`** 신설. Cool/QTE_Cool 슬롯만 Cache 에서 `Set_SweepMode(GAUGE_V)` 강제(세로 차오름, `g_fGaugeProgress` 사용). `g_fGaugeVertical` 잔재(uniform/Set_GaugeVertical/m_bGaugeVertical)는 미사용·미바인딩이라 무해(추후 정리).

**흰 화면 진짜 원인 (사용자 규명)**: `DEFERRED::COMBINED_LIGHT_DEPTH` 를 `FORCE_ALPHA` **앞에** 중간 삽입 → FORCE_ALPHA pass index 밀림 → `Begin(FORCE_ALPHA)` 가 엉뚱한 pass → Deferred 합성 깨짐 → GameApp 흰 화면. COMBINED_LIGHT_DEPTH 를 FORCE_ALPHA 뒤(END 앞)로 이동해 정상화. → **교훈: 셰이더 pass 와 1:1 매핑 enum(DEFERRED/RENDERID)은 END 앞에만 추가** (메모리 `feedback_shader_enum_pass_index`).

**기타 버그 수정**:
- `eBoxSlots` QUEST 오염: 퀘스트 eSlots 추가 시 앵커 `HUD_SLOT::SKILL_R_KEYBOX,` 가 eBoxSlots 에도 존재해 `.Replace`(전체 매치)로 QUEST_ALARM/UNDERLINE 이 BOX 강제 목록에 삽입됨 → Quest_Alarm 이 회색 박스로 표시. eBoxSlots 에서 제거. (메모리 `feedback_powershell_replace_anchor`)
- `CustomFont::Initialize` 에 `SetDefaultCharacter(L'?')` — SpriteFont 글리프 없는 글자에서 `FindGlyph` std::runtime_error 크래시 방지. (한글 전체는 makespritefont 재생성 권장)
- QTE 윈도우 활성 시 `m_bCombatInput=true`+`Set_PlayerBars_Visible(true)` 갱신 → 윈도우 동안 전체 HUD 동반 표시(회피만 했을 때 다른 HUD 사라지는 현상 해소).

**Shadow (사용자 작업, 동시 진행)**: Renderer 에 `Target_LightDepth`/`MRT_ShadowObjects`/`Render_Shadow()` + `RENDERID::SHADOW/NONLIGHT` + `Shadow.{h,cpp}` + `DEFERRED::COMBINED_LIGHT_DEPTH` + Shader_Deferred 확장.

**진행 상태**: R8-A(스킬5슬롯) / R8-B(퀘스트) 코드+저작 완료. **다음 = R8-C(콤보), R8-D(데미지 폰트)**.

---

## 9. uiscene 저작 최종 가이드 (SSOT, 2026-05-25 확정) — 총 34개

**공통**: visible=**OFF** 전부 / ProtoLevel=**GAMEPLAY** / ProtoTag 풀네임 = `Prototype_Component_Texture_HUD_` + 값 / 텍스트 FontTag=기존 HUD, HAlign=CENTER·VAlign=MIDDLE / **SweepMode 전부 NONE**(Cool 세로게이지·KeyBox BOX 는 코드 강제).

### 스킬 슬롯 5개 (C/F/Q/E/R)
X: C=930 F=1005 Q=1080 E=1155 R=1230 / Y: 660(중심), 705(키 하단). 같은 슬롯 레이어는 같은 X·Y 겹침, z 순서만 유지.

| 레이어 | 이름 | ProtoTag | 크기 | Y | z |
|---|---|---|---|---|---|
| Base | HUD_Skill_?_Base | Skill_Base (R: Skill_UT_Base) | 64 | 660 | 10 |
| Icon | HUD_Skill_?_Icon | 무기별 ↓ | 52 | 660 | 11 |
| Cool | HUD_Skill_?_Cool | Skill_Cool | 64 | 660 | 12 |
| Active | HUD_Skill_?_Active | Skill_Active (R: Skill_Ultimate) | 64 | 660 | 13 |
| KeyBox | HUD_Skill_?_KeyBox | Skill_Base | 28×24 | 705 | 10 |
| Key | HUD_Skill_?_Key | Text: C/F/Q/E/R | - | 705 | 11 |

Icon ProtoTag: C=`Skill_Icon_WpnKasaka` / F=`Skill_Icon_F_Kasaka` / Q=`Skill_Icon_Q` / E=`Skill_Icon_E_Kasaka` / R=`Skill_Icon_R`. (C·F·E 는 무기 스왑 시 코드가 KK 로 교체, Q·R 고정)

### QTE 슬롯 (위치 자유 — 회피 프롬프트, 예 X≈640 Y≈520)
| 레이어 | 이름 | ProtoTag | 크기 | z |
|---|---|---|---|---|
| Frame(기본틀) | HUD_QTE_Frame | Skill_Base | 80 | 10 |
| Active(글로우) | HUD_QTE_Active | QTE_Active | 80 | 13 |
| Icon | HUD_QTE_Icon | QTE_Icon | 64 | 11 |
| Cool | HUD_QTE_Cool | Skill_Cool | 64 | 12 |
| KeyBox | HUD_QTE_KeyBox | Skill_Base | 28×24 | 10 |
| Key | HUD_QTE_Key | Text: Shift | - | 11 |

**토글 요약** (2026-05-25 정정):
- 스킬 슬롯: CombatInput 중 Base/Icon/KeyBox/Key 표시. **Active** = R 은 쿨X 시 상시 글로우 / C·F·Q·E 는 쿨다운 **완료 순간** `SKILL_ACTIVE_FLASH`(0.6s) 플래시 후 사라짐(`m_fPrevSkillCooldown` 전이 감지). **Cool** = 쿨다운 중 세로 차오름.
- QTE = `Is_QTEWindowActive()` 윈도우 중: **Frame(=Skill_Base 기본틀)·Icon** 표시. **Active(=QTE_Active 글로우)** = 쿨X(`bQTEReady`) 시 / **Cool** = 쿨 중 차오름(`Get_QTECooldownTimer/Max`) / KeyBox·Key = 쿨X 시. 윈도우 닫히면 전부 숨김.
- QTE 쿨다운(5s) > 윈도우(1.5s) → Cool 은 윈도우 동안만 보임(결정 A).

---

## 10. R8-B 퀘스트 목표 (우측 상단) — 2026-05-25
정적 표시. 다른 HUD 와 동일 정책 = **OFF 기본 + CombatInput 시 켜기**(`Set_PlayerBars_Visible` 연동, 5초 후 숨김).
- enum: `QUEST_ALARM`, `QUEST_UNDERLINE` / Loader: `Quest_Alarm`(Icon_BattleMission_Hud_Alarm), `Quest_Underline`(back_light_line)
- 텍스트 3개 `m_pQuestText[3]`(Title/Objective/Collect) Cache_UIs 캐싱 + Set_PlayerBars_Visible 토글. eSlots 에 QUEST_ALARM/UNDERLINE 추가.
- 코드는 토글만 — 텍스트 내용 정적(향후 퀘스트 시스템 시 동적 바인딩).

### 저작 (visible=OFF 전부, ProtoLevel=GAMEPLAY, 우측 정렬 X≈1270)
| 이름 | 타입 | ProtoTag / 텍스트 | 정렬 | Y |
|---|---|---|---|---|
| HUD_Quest_Alarm | Image | Quest_Alarm | - | 55(제목 왼쪽) |
| HUD_Quest_Title | Text | 다른 구역으로 이동 | RIGHT | 55 |
| HUD_Quest_Underline | Image | Quest_Underline | - | 75 |
| HUD_Quest_Objective | Text | 지정된 목표 지점으로 이동 | RIGHT | 92 |
| HUD_Quest_Collect | Text | 수집 현황 9 / 10 | RIGHT | 115 |

※ 한글 텍스트 → SpriteFont 에 한글 글리프 필요(없으면 makespritefont 로 한글 범위 포함 재생성).

### border 라인 = 절차 단색 (UI_SWEEP_MODE::FILL)
`back_light_line` 은 글로우 이펙트라 border 부적합 → 절차 단색 채움 도입.
- `UI_SWEEP_MODE::FILL` + `PS_FILL`(g_vColor 단색) + `FillPass`(pass 6). CUI_Image Render FILL→pass 6.
- `HUD_Quest_Underline` 은 **코드(Cache)가 `Set_SweepMode(FILL)` + `Set_Color(1,1,1,0.85)` 강제** → uiscene 에선 얇은 사각형(예 220×2) 일반 Image 로 배치(ProtoTag 무관=Skill_Base, SweepMode/색 입력 불필요). 색/투명도는 `Set_Color` 값으로 조정.
