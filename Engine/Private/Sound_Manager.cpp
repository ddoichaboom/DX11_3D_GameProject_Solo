#include "Sound_Manager.h"
#include "FMOD/fmod.hpp"

CSound_Manager::CSound_Manager()
{
}

HRESULT CSound_Manager::Initialize(const _tchar* pRootPath)
{
	if (FMOD_OK != FMOD::System_Create(&m_pSystem))
		return E_FAIL;

	if (nullptr == m_pSystem)
		return E_FAIL;

	if (FMOD_OK != m_pSystem->init(64, FMOD_INIT_NORMAL, nullptr))
		return E_FAIL;

	if (FAILED(Ready_ChannelGroups()))
		return E_FAIL;

	if (nullptr != pRootPath)
	{
		if (FAILED(Load_SoundFiles(pRootPath)))
			return E_FAIL;
	}

	return S_OK;
}

void CSound_Manager::Update()
{
	if (nullptr != m_pSystem)
		m_pSystem->update();
}

HRESULT CSound_Manager::Load_SoundFiles(const _tchar* pRootPath)
{
	if (nullptr == m_pSystem || nullptr == pRootPath)
		return E_FAIL;

	if (FAILED(Scan_SoundFiles(pRootPath, TEXT(""))))
		return E_FAIL;

	Update();
	return S_OK;
}

HRESULT CSound_Manager::Play_Sound(const _wstring& strSoundKey, SOUND_CHANNEL eChannel, _float fVolume, _bool bLoop)
{
	if (nullptr == m_pSystem)
		return E_FAIL;

	FMOD::Sound* pSound = Find_Sound(strSoundKey);
	if (nullptr == pSound)
		return E_FAIL;

	const _int iChannel = ETOI(eChannel);
	if (0 > iChannel || ETOI(SOUND_CHANNEL::END) <= iChannel)
		return E_FAIL;

	pSound->setMode(true == bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	FMOD::Channel* pChannel = nullptr;
	if (FMOD_OK != m_pSystem->playSound(pSound, m_ChannelGroups[iChannel], false, &pChannel))
		return E_FAIL;

	if (nullptr != pChannel)
	{
		pChannel->setVolume(fVolume);
		m_Channels[iChannel] = pChannel;
	}

	return S_OK;
}

HRESULT CSound_Manager::Play_BGM(const _wstring& strSoundKey, _float fVolume, _bool bLoop)
{
	Stop_Sound(SOUND_CHANNEL::BGM);
	return Play_Sound(strSoundKey, SOUND_CHANNEL::BGM, fVolume, bLoop);
}

void CSound_Manager::Stop_Sound(SOUND_CHANNEL eChannel)
{
	const _int iChannel = ETOI(eChannel);
	if (0 > iChannel || ETOI(SOUND_CHANNEL::END) <= iChannel)
		return;

	if (nullptr != m_ChannelGroups[iChannel])
		m_ChannelGroups[iChannel]->stop();

	m_Channels[iChannel] = nullptr;
}

void CSound_Manager::Stop_All()
{
	for (_int i = 0; i < ETOI(SOUND_CHANNEL::END); ++i)
	{
		if (nullptr != m_ChannelGroups[i])
			m_ChannelGroups[i]->stop();
		m_Channels[i] = nullptr;
	}
}

void CSound_Manager::Set_ChannelVolume(SOUND_CHANNEL eChannel, _float fVolume)
{
	const _int iChannel = ETOI(eChannel);
	if (0 > iChannel || ETOI(SOUND_CHANNEL::END) <= iChannel)
		return;

	if (nullptr != m_ChannelGroups[iChannel])
		m_ChannelGroups[iChannel]->setVolume(fVolume);
}

_bool CSound_Manager::Is_Playing(SOUND_CHANNEL eChannel) const
{
	const _int iChannel = ETOI(eChannel);
	if (0 > iChannel || ETOI(SOUND_CHANNEL::END) <= iChannel)
		return false;

	if (nullptr == m_ChannelGroups[iChannel])
		return false;

	bool bPlaying = false;
	m_ChannelGroups[iChannel]->isPlaying(&bPlaying);

	return bPlaying;
}

HRESULT CSound_Manager::Ready_ChannelGroups()
{
	if (nullptr == m_pSystem)
		return E_FAIL;

	if (FMOD_OK != m_pSystem->getMasterChannelGroup(&m_ChannelGroups[ETOI(SOUND_CHANNEL::MASTER)]))
		return E_FAIL;

	struct CHANNEL_GROUP_DESC
	{
		SOUND_CHANNEL eChannel;
		const char* pName;
	};

	static const CHANNEL_GROUP_DESC Groups[] =
	{
		{ SOUND_CHANNEL::BGM, "BGM" },
		{ SOUND_CHANNEL::SFX, "SFX" },
		{ SOUND_CHANNEL::PLAYER, "PLAYER" },
		{ SOUND_CHANNEL::MONSTER, "MONSTER" },
		{ SOUND_CHANNEL::WEAPON, "WEAPON" },
		{ SOUND_CHANNEL::UI, "UI" },
	};

	FMOD::ChannelGroup* pMaster = m_ChannelGroups[ETOI(SOUND_CHANNEL::MASTER)];

	for (const CHANNEL_GROUP_DESC& Desc : Groups)
	{
		FMOD::ChannelGroup* pGroup = nullptr;
		if (FMOD_OK != m_pSystem->createChannelGroup(Desc.pName, &pGroup))
			return E_FAIL;

		m_ChannelGroups[ETOI(Desc.eChannel)] = pGroup;

		if (nullptr != pMaster)
			pMaster->addGroup(pGroup);
	}

	return S_OK;
}

HRESULT CSound_Manager::Scan_SoundFiles(const _wstring& strRootPath, const _wstring& strRelativePath)
{
	_wstring strSearchPath = strRootPath;
	if (false == strSearchPath.empty() && TEXT('\\') != strSearchPath.back() && TEXT('/') != strSearchPath.back())
		strSearchPath += TEXT("\\");

	if (false == strRelativePath.empty())
		strSearchPath += strRelativePath + TEXT("\\");

	WIN32_FIND_DATA FindData{};
	HANDLE hFind = FindFirstFile((strSearchPath + TEXT("*.*")).c_str(), &FindData);
	if (INVALID_HANDLE_VALUE == hFind)
		return S_OK;

	do
	{
		const _wstring strFileName = FindData.cFileName;
		if (TEXT(".") == strFileName || TEXT("..") == strFileName)
			continue;

		const _wstring strChildRelativePath = true == strRelativePath.empty() ?
			strFileName :
			strRelativePath + TEXT("\\") + strFileName;

		if (0 != (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (FAILED(Scan_SoundFiles(strRootPath, strChildRelativePath)))
			{
				FindClose(hFind);
				return E_FAIL;
			}
			continue;
		}

		if (false == Is_SoundFile(strFileName))
			continue;

		_wstring strFilePath = strRootPath;
		if (false == strFilePath.empty() && TEXT('\\') != strFilePath.back() && TEXT('/') != strFilePath.back())
			strFilePath += TEXT("\\");
		strFilePath += strChildRelativePath;

		if (FAILED(Register_SoundFile(strFilePath, strChildRelativePath)))
		{
			FindClose(hFind);
			return E_FAIL;
		}
	}
	while (FindNextFile(hFind, &FindData));

	FindClose(hFind);
	return S_OK;
}

HRESULT CSound_Manager::Register_SoundFile(const _wstring& strFilePath, const _wstring& strRelativeKey)
{
	const std::string strPath = To_MultiBytePath(strFilePath);
	if (strPath.empty())
		return E_FAIL;

	FMOD_MODE eMode = FMOD_2D | FMOD_DEFAULT;
	if (true == Is_BGMFile(Get_FileName(strFilePath)))
		eMode = FMOD_2D | FMOD_CREATESTREAM | FMOD_LOOP_NORMAL;

	FMOD::Sound* pSound = nullptr;
	if (FMOD_OK != m_pSystem->createSound(strPath.c_str(), eMode, nullptr, &pSound))
		return E_FAIL;

	const _wstring strFileName = Get_FileName(strFilePath);

	Register_Key(Normalize_Key(strRelativeKey), pSound);
	Register_Key(strFileName, pSound);
	Register_Key(Get_Stem(strFileName), pSound);

	return S_OK;
}

void CSound_Manager::Register_Key(const _wstring& strKey, FMOD::Sound* pSound)
{
	if (strKey.empty() || nullptr == pSound)
		return;

	if (m_Sounds.end() == m_Sounds.find(strKey))
		m_Sounds.emplace(strKey, pSound);
}

FMOD::Sound* CSound_Manager::Find_Sound(const _wstring& strSoundKey) const
{
	auto iter = m_Sounds.find(strSoundKey);
	if (m_Sounds.end() != iter)
		return iter->second;

	const _wstring strNormalizedKey = Normalize_Key(strSoundKey);

	iter = m_Sounds.find(strNormalizedKey);
	if (m_Sounds.end() != iter)
		return iter->second;

	return nullptr;
}

_bool CSound_Manager::Is_SoundFile(const _wstring& strFileName)
{
	const size_t iDot = strFileName.find_last_of(TEXT('.'));
	if (_wstring::npos == iDot)
		return false;

	_wstring strExt = strFileName.substr(iDot);
	std::transform(strExt.begin(), strExt.end(), strExt.begin(), ::towlower);

	return TEXT(".wav") == strExt || TEXT(".mp3") == strExt || TEXT(".ogg") == strExt;
}

_bool CSound_Manager::Is_BGMFile(const _wstring& strFileName)
{
	return _wstring::npos != strFileName.find(TEXT("BGM")) ||
		_wstring::npos != strFileName.find(TEXT("Bgm")) ||
		_wstring::npos != strFileName.find(TEXT("bgm"));
}

_wstring CSound_Manager::Get_FileName(const _wstring& strPath)
{
	const size_t iSlash = strPath.find_last_of(TEXT("\\/"));
	if (_wstring::npos == iSlash)
		return strPath;

	return strPath.substr(iSlash + 1);
}

_wstring CSound_Manager::Get_Stem(const _wstring& strFileName)
{
	const size_t iDot = strFileName.find_last_of(TEXT('.'));
	if (_wstring::npos == iDot)
		return strFileName;

	return strFileName.substr(0, iDot);
}

std::string CSound_Manager::To_MultiBytePath(const _wstring& strPath)
{
	if (strPath.empty())
		return {};

	const int iLength = WideCharToMultiByte(CP_ACP, 0, strPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (0 == iLength)
		return {};

	std::string strResult;
	strResult.resize(static_cast<size_t>(iLength));
	WideCharToMultiByte(CP_ACP, 0, strPath.c_str(), -1, &strResult[0], iLength, nullptr, nullptr);

	if (false == strResult.empty() && '\0' == strResult.back())
		strResult.pop_back();

	return strResult;
}

_wstring CSound_Manager::Normalize_Key(_wstring strKey)
{
	std::replace(strKey.begin(), strKey.end(), L'\\', L'/');
	return strKey;
}

CSound_Manager* CSound_Manager::Create(const _tchar* pRootPath)
{
	CSound_Manager* pInstance = new CSound_Manager();

	if (FAILED(pInstance->Initialize(pRootPath)))
	{
		MSG_BOX("Failed to Created : CSound_Manager");
		Safe_Release(pInstance);
	}

	return pInstance;
}

void CSound_Manager::Free()
{
	__super::Free();

	Stop_All();

	std::set<FMOD::Sound*> ReleasedSounds;
	for (auto& Pair : m_Sounds)
	{
		if (nullptr == Pair.second)
			continue;

		if (ReleasedSounds.end() != ReleasedSounds.find(Pair.second))
			continue;

		Pair.second->release();
		ReleasedSounds.insert(Pair.second);
	}
	m_Sounds.clear();

	for (_int i = 0; i < ETOI(SOUND_CHANNEL::END); ++i)
	{
		if (i == ETOI(SOUND_CHANNEL::MASTER))
			continue;

		if (nullptr != m_ChannelGroups[i])
		{
			m_ChannelGroups[i]->release();
			m_ChannelGroups[i] = nullptr;
		}
	}

	if (nullptr != m_pSystem)
	{
		m_pSystem->close();
		m_pSystem->release();
		m_pSystem = nullptr;
	}
}
