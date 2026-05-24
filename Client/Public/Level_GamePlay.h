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

private:
	_bool						Apply_PlayerSpawnFromCell(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, _int iCellIndex);
	_bool						Apply_PlayerSpawnPoint(CPlayer::PLAYER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT* pSpawnPoint);
	CNavMesh*					Find_GamePlayNavMesh();
	const _tchar*				Get_MonsterPrototypeTag(SPAWN_TYPE eType) const;
	_bool						Apply_MonsterSpawnPoint(CMonster::MONSTER_DESC& Desc, CNavMesh* pNavMesh, const SPAWN_POINT& SpawnPoint);
	
	CPlayer*					Find_FirstPlayer() const;

#ifdef _DEBUG
	CMonster*					Find_FirstBossMonster() const;
#endif
private:
	SCENE_DATA					m_SceneData = {};
	_bool						m_bSceneDataLoaded = { false };

public:
	static CLevel_GamePlay*		Create(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
	virtual void				Free() override;
};

NS_END
