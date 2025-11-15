#pragma once

#include <config.hpp>
#include <fstream>
#include <mutex>
#include <vector>

extern "C"
{
#include "finders.h"
#include "generator.h"
#include "inttypes.h"
#include "quadbase.h"
}

// Returns temple type if biome is valid and temple type is selected:
// 1 -> DesertPyramid
// 2 -> JungleTemple
// 3 -> WitchHut
inline int isViableTemplePos(Generator *g, int x, int z)
{
    int biomeAtCenter = getBiomeAt(g, BIOME_QUERY_SCALE, x + HALF_CHUNK, QUERY_Y, z + HALF_CHUNK);

    if (biomeAtCenter == desert || biomeAtCenter == desert_hills)
    {
        return (SELECTED_TEMPLE_TYPES & TT_DESERT) ? 1 : 0;
    }
    else if (biomeAtCenter == jungle || biomeAtCenter == jungle_hills)
    {
        return (SELECTED_TEMPLE_TYPES & TT_JUNGLE) ? 2 : 0;
    }
    else if (biomeAtCenter == swampland)
    {
        return (SELECTED_TEMPLE_TYPES & TT_WITCH) ? 3 : 0;
    }

    return 0;
}

// Counts witch-only spawning spaces for a given temple
inline int countSwampSpawnBlocks(Generator *g, int startX, int startZ, int templeType)
{
    PieceSize piece;
    int multiplier = 0;

    if (templeType == 1)
    {
        piece = DESERT_PYRAMID;
        multiplier = 5;
    }
    else if (templeType == 2)
    {
        piece = JUNGLE_TEMPLE;
        multiplier = 4;
    }
    else
    {
        piece = WITCH_HUT;
        multiplier = 2;
    }

    int endX = startX + piece.w;
    int endZ = startZ + piece.d;

    Range r;
    r.scale = BIOME_QUERY_SCALE;
    r.x = startX;
    r.z = startZ;
    r.sx = piece.w;
    r.sz = piece.d;
    r.y = QUERY_Y;
    r.sy = 1;

    int *biomeIds = allocCache(g, r);
    if (!biomeIds)
    {
        // Allocation failed: fallback to per-block loop
        int swampCount = 0;
        for (int x = startX; x < endX; ++x)
            for (int z = startZ; z < endZ; ++z)
                if (getBiomeAt(g, BIOME_QUERY_SCALE, x, QUERY_Y, z) == swampland)
                    ++swampCount;
        return swampCount * multiplier;
    }

    genBiomes(g, biomeIds, r);

    int swampCount = 0;
    for (int iz = 0; iz < r.sz; ++iz)
    {
        int base = iz * r.sx;
        for (int ix = 0; ix < r.sx; ++ix)
        {
            int biome = biomeIds[base + ix];
            if (biome == swampland)
                ++swampCount;
        }
    }

    free(biomeIds);
    return swampCount * multiplier;
}
