#include "SceneSerializer.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"

namespace
{
    static constexpr char SCENEDATA_MAGIC[4] = { 'S', 'L', 'S', 'C' };
    static constexpr _uint SCENEDATA_VERSION_MIN = { 1 };
    static constexpr _uint SCENEDATA_VERSION_LATEST = { 5 };

    HRESULT Write_SpawnPoint(CBinaryWriter& Writer, const SPAWN_POINT& Point)
    {
        const _uint iType = static_cast<_uint>(Point.eType);

        if (FAILED(Writer.Write(iType)))
            return E_FAIL;

        if (FAILED(Writer.Write(Point.iNavCellIndex)))
            return E_FAIL;

        if (FAILED(Writer.Write(Point.vPosition)))
            return E_FAIL;

        if (FAILED(Writer.Write(Point.vRotationDeg)))
            return E_FAIL;

        if (FAILED(Writer.WriteArray(Point.szName, sizeof(_tchar), MAX_PATH)))
            return E_FAIL;

        if (FAILED(Writer.Write(Point.iLevel)))                                        
            return E_FAIL;

        if (FAILED(Writer.WriteArray(Point.szDisplayName, sizeof(_tchar), MAX_PATH)))  
            return E_FAIL;

        return S_OK;
    }

    HRESULT Read_SpawnPoint(CBinaryReader& Reader, SPAWN_POINT* pOutPoint, _uint iVersion)
    {
        if (nullptr == pOutPoint)
            return E_FAIL;

        _uint iType = {};

        if (FAILED(Reader.Read(&iType)))                                                    
            return E_FAIL;
        if (iType >= static_cast<_uint>(SPAWN_TYPE::END))                                   
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutPoint->iNavCellIndex)))                                 
            return E_FAIL;
        if (FAILED(Reader.Read(&pOutPoint->vPosition)))                                     
            return E_FAIL;
        if (FAILED(Reader.Read(&pOutPoint->vRotationDeg)))                                  
            return E_FAIL;
        if (FAILED(Reader.ReadArray(pOutPoint->szName, sizeof(_tchar), MAX_PATH)))          
            return E_FAIL;

        if (iVersion >= 2)
        {
            if (FAILED(Reader.Read(&pOutPoint->iLevel)))                                    
                return E_FAIL;
            if (FAILED(Reader.ReadArray(pOutPoint->szDisplayName, sizeof(_tchar), MAX_PATH)))
                return E_FAIL;
        }
        else
        {
            // v1 fallback
            pOutPoint->iLevel = 1;
            pOutPoint->szDisplayName[0] = 0;
        }

        pOutPoint->eType = static_cast<SPAWN_TYPE>(iType);
        pOutPoint->szName[MAX_PATH - 1] = 0;
        pOutPoint->szDisplayName[MAX_PATH - 1] = 0;

        return S_OK;
    }

    HRESULT Write_SceneLight(CBinaryWriter& Writer, const SCENE_LIGHT& Light)
    {
        const _uint iType = static_cast<_uint>(Light.eType);

        if (FAILED(Writer.Write(iType)))
            return E_FAIL;

        if (FAILED(Writer.WriteArray(Light.szName, sizeof(_tchar), MAX_PATH)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.vDiffuse)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.vAmbient)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.vSpecular)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.vDirection)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.vPosition)))
            return E_FAIL;

        if (FAILED(Writer.Write(Light.fRange)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Read_SceneLight(CBinaryReader& Reader, SCENE_LIGHT* pOutLight, _uint iVersion)
    {
        if (nullptr == pOutLight)
            return E_FAIL;

        _uint iType = {};

        if (FAILED(Reader.Read(&iType)))
            return E_FAIL;

        if (iType >= static_cast<_uint>(LIGHT::END))
            return E_FAIL;

        if (iVersion >= 4)
        {
            if (FAILED(Reader.ReadArray(pOutLight->szName, sizeof(_tchar), MAX_PATH)))
                return E_FAIL;
        }
        else
        {
            pOutLight->szName[0] = 0;
        }

        if (FAILED(Reader.Read(&pOutLight->vDiffuse)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutLight->vAmbient)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutLight->vSpecular)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutLight->vDirection)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutLight->vPosition)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutLight->fRange)))
            return E_FAIL;

        pOutLight->eType = static_cast<LIGHT>(iType);
        pOutLight->szName[MAX_PATH - 1] = 0;

        return S_OK;
    }

    HRESULT Write_CamColliderFace(CBinaryWriter& Writer, const CAMCOLLIDER_FACE& Face)
    {
        if (FAILED(Writer.WriteArray(Face.iVertexIndices, sizeof(_int), 3)))
            return E_FAIL;

        if (FAILED(Writer.Write(Face.vNormal)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Read_CamColliderFace(CBinaryReader& Reader, CAMCOLLIDER_FACE* pOutFace)
    {
        if (nullptr == pOutFace)
            return E_FAIL;

        if (FAILED(Reader.ReadArray(pOutFace->iVertexIndices, sizeof(_int), 3)))
            return E_FAIL;

        if (FAILED(Reader.Read(&pOutFace->vNormal)))
            return E_FAIL;

        return S_OK;
    }
}

HRESULT CSceneSerializer::Save(const _tchar* pSceneDataPath, const SCENE_DATA& SceneData)
{
    if (nullptr == pSceneDataPath || 0 == pSceneDataPath[0])
        return E_FAIL;

    CBinaryWriter Writer;

    if (FAILED(Writer.Open(pSceneDataPath)))
        return E_FAIL;

    const _uint iVersion = SCENEDATA_VERSION_LATEST;
    const _uint iNumSpawnPoints = static_cast<_uint>(SceneData.SpawnPoints.size());
    const _uint iNumSceneLights = static_cast<_uint>(SceneData.SceneLights.size());
    const _uint iNumCamColliderVertices = static_cast<_uint>(SceneData.CamColliderVertices.size());
    const _uint iNumCamColliderFaces = static_cast<_uint>(SceneData.CamColliderFaces.size());

    if (FAILED(Writer.WriteMagic(SCENEDATA_MAGIC, 4)))
        return E_FAIL;

    if (FAILED(Writer.WriteVersion(iVersion)))
        return E_FAIL;

    if (FAILED(Writer.WriteArray(SceneData.szNavDataPath, sizeof(_tchar), MAX_PATH)))
        return E_FAIL;

    if (FAILED(Writer.Write(iNumSpawnPoints)))
        return E_FAIL;

    for (const SPAWN_POINT& Point : SceneData.SpawnPoints)
    {
        if (FAILED(Write_SpawnPoint(Writer, Point)))
            return E_FAIL;
    }

    if (FAILED(Writer.Write(iNumSceneLights)))
        return E_FAIL;

    for (const SCENE_LIGHT& Light : SceneData.SceneLights)
    {
        if (FAILED(Write_SceneLight(Writer, Light)))
            return E_FAIL;
    }

    if (FAILED(Writer.Write(iNumCamColliderVertices)))
        return E_FAIL;

    for (const _float3& vPosition : SceneData.CamColliderVertices)
    {
        if (FAILED(Writer.Write(vPosition)))
            return E_FAIL;
    }

    if (FAILED(Writer.Write(iNumCamColliderFaces)))
        return E_FAIL;

    for (const CAMCOLLIDER_FACE& Face : SceneData.CamColliderFaces)
    {
        if (FAILED(Write_CamColliderFace(Writer, Face)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT CSceneSerializer::Load(const _tchar* pSceneDataPath, SCENE_DATA* pOutSceneData)
{
    if (nullptr == pSceneDataPath || 0 == pSceneDataPath[0] || nullptr == pOutSceneData)
        return E_FAIL;

    CBinaryReader Reader;

    if (FAILED(Reader.Open(pSceneDataPath)))
        return E_FAIL;

    _uint iVersion = {};
    _uint iNumSpawnPoints = {};
    _uint iNumSceneLights = {};
    _uint iNumCamColliderVertices = {};
    _uint iNumCamColliderFaces = {};

    if (FAILED(Reader.ReadMagic(SCENEDATA_MAGIC, 4)))
        return E_FAIL;

    if (FAILED(Reader.ReadVersion(&iVersion, SCENEDATA_VERSION_MIN, SCENEDATA_VERSION_LATEST)))
        return E_FAIL;

    SCENE_DATA SceneData{};

    if (FAILED(Reader.ReadArray(SceneData.szNavDataPath, sizeof(_tchar), MAX_PATH)))
        return E_FAIL;

    SceneData.szNavDataPath[MAX_PATH - 1] = 0;

    if (FAILED(Reader.Read(&iNumSpawnPoints)))
        return E_FAIL;

    SceneData.SpawnPoints.clear();
    SceneData.SpawnPoints.resize(iNumSpawnPoints);

    for (SPAWN_POINT& Point : SceneData.SpawnPoints)
    {
        if (FAILED(Read_SpawnPoint(Reader, &Point, iVersion)))
            return E_FAIL;
    }

    SceneData.SceneLights.clear();

    if (iVersion >= 3)
    {
        if (FAILED(Reader.Read(&iNumSceneLights)))
            return E_FAIL;

        SceneData.SceneLights.resize(iNumSceneLights);

        for (SCENE_LIGHT& Light : SceneData.SceneLights)
        {
            if (FAILED(Read_SceneLight(Reader, &Light, iVersion)))
                return E_FAIL;
        }
    }

    SceneData.CamColliderVertices.clear();
    SceneData.CamColliderFaces.clear();

    if (iVersion >= 5)
    {
        if (FAILED(Reader.Read(&iNumCamColliderVertices)))
            return E_FAIL;

        SceneData.CamColliderVertices.resize(iNumCamColliderVertices);

        for (_float3& vPosition : SceneData.CamColliderVertices)
        {
            if (FAILED(Reader.Read(&vPosition)))
                return E_FAIL;
        }

        if (FAILED(Reader.Read(&iNumCamColliderFaces)))
            return E_FAIL;

        SceneData.CamColliderFaces.resize(iNumCamColliderFaces);

        for (CAMCOLLIDER_FACE& Face : SceneData.CamColliderFaces)
        {
            if (FAILED(Read_CamColliderFace(Reader, &Face)))
                return E_FAIL;
        }
    }

    *pOutSceneData = SceneData;

    return S_OK;
}

const SPAWN_POINT* CSceneSerializer::Find_FirstSpawnPoint(const SCENE_DATA& SceneData, SPAWN_TYPE eType)
{
    for (const SPAWN_POINT& Point : SceneData.SpawnPoints)
    {
        if (Point.eType == eType)
            return &Point;
    }

    return nullptr;
}
