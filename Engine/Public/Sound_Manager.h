#pragma once

#include "Base.h"

namespace FMOD
{
	class System;
	class Sound;
	class Channel;
	class ChannelGroup;
}

NS_BEGIN(Engine)

class ENGINE_DLL CSound_Manager final : public CBase
{
private:
	CSound_Manager();
	virtual ~CSound_Manager() = default;

public:
	HRESULT Initialize(const _tchar* pRootPath = TEXT("../../Resources/Audio"));
	void Update();

	HRESULT Load_SoundFiles(const _tchar* pRootPath);

	HRESULT Play_Sound(const _wstring& strSoundKey, SOUND_CHANNEL eChannel, _float fVolume = 1.f, _bool bLoop = false);
	HRESULT Play_BGM(const _wstring& strSoundKey, _float fVolume = 1.f, _bool bLoop = true);

	void Stop_Sound(SOUND_CHANNEL eChannel);
	void Stop_All();
	void Set_ChannelVolume(SOUND_CHANNEL eChannel, _float fVolume);
	_bool Is_Playing(SOUND_CHANNEL eChannel) const;

private:
	HRESULT Ready_ChannelGroups();
	HRESULT Scan_SoundFiles(const _wstring& strRootPath, const _wstring& strRelativePath);
	HRESULT Register_SoundFile(const _wstring& strFilePath, const _wstring& strRelativeKey);
	void Register_Key(const _wstring& strKey, FMOD::Sound* pSound);
	FMOD::Sound* Find_Sound(const _wstring& strSoundKey) const;

	static _bool Is_SoundFile(const _wstring& strFileName);
	static _bool Is_BGMFile(const _wstring& strFileName);
	static _wstring Get_FileName(const _wstring& strPath);
	static _wstring Get_Stem(const _wstring& strFileName);
	static std::string To_MultiBytePath(const _wstring& strPath);
	static _wstring Normalize_Key(_wstring strKey);

private:
	FMOD::System* m_pSystem = { nullptr };
	std::map<_wstring, FMOD::Sound*> m_Sounds;
	FMOD::Channel* m_Channels[ETOI(SOUND_CHANNEL::END)] = {};
	FMOD::ChannelGroup* m_ChannelGroups[ETOI(SOUND_CHANNEL::END)] = {};

public:
	static CSound_Manager* Create(const _tchar* pRootPath = TEXT("../../Resources/Audio"));
	virtual void Free() override;
};

NS_END
