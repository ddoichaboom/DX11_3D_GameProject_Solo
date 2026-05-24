# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working in this repository.

## Reference Projects

- 참고 프로젝트 1 (4-Project DLL 원본): `C:\Users\chaho\ddoichaboom\CPP_STUDY_3D\Framework_4Proj\Framework\CLAUDE.md`
- 참고 프로젝트 2 (3-Project 원본): `C:\Users\chaho\ddoichaboom\CPP_STUDY_3D\Framework\CLAUDE.md`
- 수업 코드 (Assimp/Model/Animation 원본, 최신 15일차): `C:\Users\denni\ddoichaboom\CPP_STUDY_3D\수업 파일\Files\15일차` — 경로 무효 시 `C:\Users\denni\ddoichaboom\CPP_STUDY_3D\` 하위 최신 탐색
- 수업 메모 (9~14일차): `C:\Users\denni\ddoichaboom\CPP_STUDY_3D\명세서\` (9 Model, 10 Material, 11 Animation+Bone, 12 Bone-Mesh OffsetMatrix, 13 Animation+Channel, 14 키프레임 보간). 세부 코드 오류 가능 — 주제 참고용
- **수업 코드는 Assimp 를 Engine 에서 직접 사용. Solo_Leveling 은 Assimp 를 Editor 로 격리함**

## Build

- VS2022 (v143), `Framework.sln`, Debug|x64 기본
- 4 프로젝트: **Engine(DLL) ← Client(DLL) ← GameApp(EXE) / Editor(EXE)**
- Post-build: `EngineSDK_Update.bat`, `ClientSDK_Update.bat` (Inc/Lib/Bin 자동 복사)
- CRT 통일: 전 프로젝트 동적 CRT (`/MDd`, `/MD`). `/MT` 혼용 시 힙 충돌
- Build: `msbuild Framework.sln /p:Configuration=Debug /p:Platform=x64`

## Architecture

Client 를 DLL 로 만든 이유: Editor 에서 Client 의 GameObject 프로토타입을 Clone 해 쓰기 위함.

**핵심 계층** (모두 `CBase` ref counting):

- **GameInstance/서브시스템**: `CGameInstance`(singleton), `CInput_Device`(Raw Input, 2단계 static→frame), `CPipeLine`, `CGraphic_Device`
- **Component**: `CTransform(_3D/_2D)`, `CShader`, `CTexture`, `CVIBuffer(_Rect/_Terrain)`, `CModel`, `CAnimController`
- **CModel 내부**: `CMesh/CMaterial/CBone/CAnimation/CChannel` (Assimp 미사용, SLMD 바이너리 로드)
- **GameObject**: `CCamera`, `CUIObject`, `CContainerObject`(PartObject map 소유), `CPartObject`(ParentMatrix 기반 `CombinedWorldMatrix`)
- **Client**: `CPlayer`(Container) → `CBody_Player`(Anim) + `CWeapon`(Static), `CMapObject/CMapStaticObject`
- **Client StateMachine**: `CStateMachine`(Engine, abstract) ← `CPlayer_StateMachine`(Client). `_uint` opaque action ID 커널 + Client 가 CHARACTER_ACTION 매핑
- **Editor**: `CPanel` 6개(Viewport/Hierarchy/Inspector/ContentBrowser/Log/Shortcuts) + `CPanel_Manager`(singleton), `CEditorApp`, `CModel_Converter`

**Key patterns**:
- Singleton: `DECLARE_SINGLETON` / `IMPLEMENT_SINGLETON`
- Prototype/Clone: `Add_Prototype` → `Clone_Prototype(GAMEOBJECT or COMPONENT)`
- Descriptor chain: `TRANSFORM_DESC` → `GAMEOBJECT_DESC` (+ eTransformType) → 파생 DESC
- Update pipeline: `Input_Device::Update` → `Priority_Update` → `Update` → `PipeLine::Update` → `Late_Update`
- Render groups: Late_Update 에서 `Add_RenderGroup(RENDERID, this)` → Priority/NonBlend/Blend/UI 순. AddRef/Release 사이클
- DLL export: `ENGINE_DLL` / `CLIENT_DLL` (외부 공개 클래스)

**Vertex ↔ Shader**:
| Vertex | Shader | 용도 |
|---|---|---|
| VTXTEX | Shader_VtxTex | BackGround/UI |
| VTXNORTEX | Shader_VtxNorTex | Terrain (Phong) |
| VTXMESH | Shader_VtxMesh | Static Model |
| VTXANIMMESH | Shader_VtxAnimMesh | Skinned (BoneIndices/Weights) |

## Project Layout

```
Framework/
├── Engine/           Public/ Private/ ThirdPartyLib/ Bin/
├── Client/           Public/ Private/ Bin/
├── GameApp/Default/  wWinMain, 60FPS loop
├── Editor/           Default/ Public/ Private/ ThirdPartyLib/ ImGui/ ImGuizmo/ assimp/
├── Resources/        ShaderFiles/ Textures/ Models/
├── EngineSDK/  ClientSDK/   post-build 자동 복사
├── 명세서/           설계/진행 기록 (한국어)
└── Framework.sln
```

런타임 리소스 경로 패턴: `TEXT("../../Resources/...")` (프로젝트 속성에 Resources/ 추가 불필요)

## Coding Conventions

- Class `C` prefix, member `m_` prefix, method PascalCase
- Namespace: `Engine` / `Client` / `Editor` via `NS_BEGIN` / `NS_END`
- Singleton `final`, init 함수 `HRESULT` 반환
- `Safe_Delete` / `Safe_Release` / `Safe_AddRef` — raw `delete` 금지
- `ETOI` / `ETOUI` — enum → int/uint 캐스팅

## Gotchas (핵심 주의사항)

- **m_WorldMatrix 초기화**: 반드시 Identity. 영행렬 시 WVP 후 렌더 안 됨
- **CTexture Clone**: 복사 생성자에서 각 SRV `Safe_AddRef()` 필수 (미적용 시 combase.dll 크래시)
- **CVIBuffer Clone**: 복사 생성자에서 `PICK_DATA` 깊은 복사 필수 (얕은 복사 시 이중 해제 크래시)
- **InputLayout ↔ 정점 구조체**: 1:1 대응 (불일치 시 크래시)
- **Level Transition**: `SUCCEEDED()` 사용 (`FAILED()` 아님)
- **CLoader 스레드**: WIC 텍스처 로딩 시 `CoInitializeEx(nullptr, 0)` 필수
- **Release 순서**: CMainApp — Device → Context → Release_Engine → GameInstance
- **WM_INPUT**: WndProc 에서 `CGameInstance::Process_RawInput(lParam)` 호출
- **Safe_Release 동작**: RefCount > 0 이면 포인터 nullptr 처리 안 됨 → 선택 해제 등에서 수동 `= nullptr` 필요
- **VIBuffer_Terrain PICK_DATA**: `Initialize_Prototype` 에서 pVerticesPos 할당은 정점 값 쓰기 **전에**
- **`::operator new/delete` 금지**: `Engine_Defines.h::#define new DBG_NEW` 때문에 컴파일 오류. void 바이트 버퍼는 `new _ubyte[size]` / `delete[] static_cast<_ubyte*>(ptr)`. `Safe_Delete_Array<T>` 는 void* 불가
- **ImGui/RTTR 충돌**: `#define new DBG_NEW` 가 ImGui/RTTR 와 충돌 → `#undef new` 필요. RTTR `registration.h` 는 Engine 헤더 전 include 또는 `#undef new` 후 include
- **C++17 필수**: `/std:c++17` (filesystem)
- **RTTR 0.9.6**: `NOMINMAX` 전처리기, `/permissive` 아니요, getter 0인자/setter 1인자 멤버 함수 패턴. abstract 등록 가능, private/protected 소멸자 concrete 는 등록 불가
- **bRestartOnEnter (CharacterAnimTable bind 마지막 필드)**: 1회성 리액션/공격 클립은 반드시 `true`. `CModel::Set_AnimationIndex_WithBlend` 가 `true` 일 때만 `Reset_TrackPosition()` 호출 → `false` 면 이전 트랙 끝 위치에서 시작해 두번째 진입부터 애니가 안 보임

## Editor / 외부 라이브러리 격리

- **격리 원칙**: Assimp(FBX→.bin 변환), RTTR(Inspector), ImGui, ImGuizmo 모두 **Editor 전용**. Engine/Client 무의존
- **Engine 측 훅 최소화**: 예) ImGuizmo 는 `CTransform::Set_WorldMatrix` 인라인 setter 한 줄만. 같은 원칙으로 Assimp/RTTR/ImGui 격리
- **CPanel_Manager 순환 참조 해결**: CPanel 이 GameInstance/Panel_Manager `Safe_AddRef` → 순환. `Release_Panels()` 수동 해제 (`EditorApp::Free` 에서 호출)
- **Viewport RT 파이프라인**: 별도 RT(Tex2D+RTV+SRV+DSV) → Begin_RT/Draw/End_RT → BackBuffer 재바인딩 → ImGui → Present. 리사이즈 시 `GetContentRegionAvail()` 비교 후 재생성
- **ImGuizmo ↔ DirectX 행우선**: `_float4x4*` 를 `reinterpret_cast<float*>` 로 그대로 전달 (transpose 불필요)
- **폴링 기반 동기화**: Inspector RTTR getter 와 Gizmo 가 매 프레임 `m_WorldMatrix` 폴링 → SSOT. dirty 플래그/이벤트 불필요
- **단축키 배치**: `ImGui::IsWindowFocused()` 가 Begin/End 스코프 내에서만 유효 → 단축키는 패널 `Render()` 에서 처리 (입력+드로잉 혼재 정상)
- **Editor 테스트 씬**: `CLevel_Editor`(빈 CLevel 셸) + `Ready_TestScene()` — Client 게임 레벨은 CLIENT_DLL export 없어 Editor 전용 빈 레벨로 대체

## CModel / SLMD 바이너리

- **아키텍처 (B안)**: Editor 만 Assimp. FBX → MODEL_DESC → SLMD `.bin` 저장. Engine/Client 는 `CModel::Create(..., pBinaryPath)` 로 `.bin` 만 로드 → Engine 의 Assimp 의존 완전 제거
- SLMD magic + version:
  - v1: 기본
  - v2: 퍼-애니메이션 `bIsLoop` / `bUseRootMotion` / `RootBoneName` + `PreTransform` 저장 (루트 모션 지원)
  - v3 (Step C 예정): AnimNotify 배열 추가. v2 파일은 빈 notify 배열로 폴백 로드
- `Play_Animation(fTimeDelta)` → `_bool` 완료 반환. loop SSOT 는 `ANIMATION_DESC::bIsLoop`
- `Load_Binary_Desc` / `Free_Binary_Desc` 쌍. 정점 버퍼는 `new _ubyte[]` (DBG_NEW 호환)
- `CModel_Converter` (Editor): aiScene → SLMD. Collect_Bones(DFS), Should_Skip_Mesh(LowMesh/NLOD 필터), Build_TexturePath(상대), 0-bone 폴백, DIFFUSE 없을 때 `_CO` 텍스처 자동 탐색

## 현재 작업 컨텍스트

- **단계 3 Player 게임플레이 진행 중**. 세부 계획: `명세서/단계3_Player_세부계획.md`
- 상위 계획서: `명세서/통합_구현계획_v2.md`
- Editor 전체 계획 (배경): `명세서/Editor_전체_구현계획.md`
- 단계 2-5 (Runtime Testbed) 완료. `CLevel_GamePlay` 는 `Ready_Layer_BackGround` + `Ready_Layer_Player` 구성

## Working with Claude Code

**Communication**: Korean and English 모두 가능

**File Modification Policy**:
- Claude Code 는 소스 파일(.cpp/.h/.hlsl 등)을 Edit/Write 로 **직접 수정 금지** (인코딩 이슈)
- **예외**: `CLAUDE.md` 와 `명세서/*.md` 는 직접 수정 가능
- 소스 변경은 마크다운 코드블록으로 제시 → 사용자가 수동 반영
- 파일 복사/이동도 Bash 직접 실행 금지 — 명령 텍스트만 안내, 사용자가 실행

**명세서 관리**: 반드시 `명세서/` 폴더 (`C:\Users\chaho\ddoichaboom\Solo_Leveling\명세서\`) 에 생성

**SubAgent**: Explore/Task 적극 활용 (컨텍스트 보호). 출력 품질 미흡 시 재실행
