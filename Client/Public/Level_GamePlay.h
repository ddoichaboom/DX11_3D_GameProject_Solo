#pragma once

#include "Client_Defines.h"
#include "Level.h"
#include "Player.h"
#include "Monster.h"

NS_BEGIN(Client)

class CLIENT_DLL CLevel_GamePlay final : public CLevel
{
private:
	CLevel_GamePlay(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual ~CLevel_GamePlay() = default;

public:
	virtual HRESULT				Initialize() override;
	virtual void				Update(_float fTimeDelta) override;
	virtual HRESULT				Render() override;

public:
	HRESULT						Ready_SceneData();
	HRESULT						Ready_Lights();
	HRESULT						Ready_Layer_Camera(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_BackGround(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_NavMesh(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_Monster(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_Player(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_Effect(const _wstring& strLayerTag);
	HRESULT						Ready_Layer_UI(const _wstring& strLayerTag);
	HRESULT						Ready_CollisionGroup();
	HRESULT						Ready_PlayerTrigger();

private:
	_bool						Apply_PlayerSpawnFromCell(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, _int iCellIndex);
	_bool						Apply_PlayerSpawnPoint(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT* pSpawnPoint);
	CNavMesh*					Find_GamePlayNavMesh() const;
	const _tchar*				Get_MonsterPrototypeTag(SPAWN_TYPE eType) const;
	_bool						Apply_MonsterSpawnPoint(CMonster::MONSTER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT& SpawnPoint);
	
	CPlayer*					Find_FirstPlayer() const;
	_int						Get_PlayerNavCellIndex() const;
	void						Update_PlayerTrigger(_float fTimeDelta);
	void						Execute_PlayerTrigger(_int iTriggerCellIndex);
	void						Begin_PlayerCutsceneTrigger();
	void						Start_PlayerCutscenePlayback();
	void						Start_PlayerCutsceneFadeOut();
	void						Finish_PlayerCutscene();
	HRESULT						Add_PlayerCutsceneVideo();
	class CGameObject*			Find_PlayerCutsceneVideo() const;
	void						Remove_PlayerCutsceneVideo();
	_bool						Move_PlayerToNavCell(_int iCellIndex);

	CMonster*					Find_FirstBossMonster() const;

private:
	SCENE_DATA					m_SceneData = {};
	_bool						m_bSceneDataLoaded = { false };
	static constexpr _int		PLAYER_CUTSCENE_TRIGGER_CELL_INDEX = 72;
	static constexpr _int		PLAYER_CUTSCENE_DEST_CELL_INDEX = 97;
	static constexpr _float		PLAYER_CUTSCENE_PLAY_TIME = 12.014f;
	static constexpr _float		PLAYER_CUTSCENE_FADE_TIME = 0.5f;
	_bool						m_bPlayerCutsceneTriggerExecuted = { false };
	_bool						m_bPlayerCutscenePlaying = { false };
	_bool						m_bPlayerCutsceneFadeOutStarted = { false };
	_float						m_fPlayerCutsceneElapsed = { 0.f };

public:
	static CLevel_GamePlay*		Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void				Free() override;
};

NS_END
