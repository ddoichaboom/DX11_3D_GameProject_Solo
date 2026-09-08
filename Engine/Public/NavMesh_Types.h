#pragma once

#include "Engine_Defines.h"

NS_BEGIN(Engine)

static constexpr _int	INVALID_INDEX		= { -1 };
static constexpr _float NAVMESH_DEFAULT_SNAP_RADIUS = { 0.5f };
static constexpr _float NAVMESH_MIN_CELL_AREA		= { 0.0001f };

enum class NAVMESH_POINT : _uint
{
	A,
	B,
	C,
	END
};

enum class NAVMESH_LINE : _uint
{
	AB,
	BC,
	CA,
	END
};

typedef struct tagNavMeshCell
{
	_int iVertexIndices[3] = {
		INVALID_INDEX,
		INVALID_INDEX,
		INVALID_INDEX
	};

	_int iNeighborIndices[3] = {
		INVALID_INDEX,
		INVALID_INDEX,
		INVALID_INDEX
	};
}NAVMESH_CELL;

typedef struct tagNavMeshSnapshot
{
	vector<_float3>			Vertices;
	vector<NAVMESH_CELL>	Cells;
}NAVMESH_SNAPSHOT;

NS_END