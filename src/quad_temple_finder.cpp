#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <mutex>
#include <algorithm>
#include <fstream>

extern "C"
{
#include "finders.h"
#include "quadbase.h"
#include "generator.h"
}

constexpr int MC_VERSION = MC_1_5;
constexpr int QUERY_Y = 64;
constexpr int BIOME_QUERY_SCALE = 1;

int check(uint64_t s48, void *data)
{
    const StructureConfig sconf = *(const StructureConfig *)data;
    return isQuadBase(sconf, s48 - sconf.salt, 148);
}

int countSwampBlocks(Generator *g, int startX, int startZ)
{
    int endX = startX + 21 - 1;
    int endZ = startZ + 21 - 1;

    int swampCount = 0;
    for (int bx = startX; bx <= endX; ++bx)
    {
        for (int bz = startZ; bz <= endZ; ++bz)
        {
            int bBiome = getBiomeAt(g, BIOME_QUERY_SCALE, bx, QUERY_Y, bz);
            if (bBiome == swampland)
                ++swampCount;
        }
    }

    return swampCount;
}

int run_quad_temple_finder(uint64_t startSeed /* = 0 */)
{
    int styp = Desert_Pyramid;

    std::ofstream log("logs/quad_temple_finder.log", std::ios::trunc);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/quad_temple_finder.log'\n");
    else
        log << "seed, total_swamp_blocks\n";

    uint64_t basecnt = 0;
    uint64_t *bases = NULL;
    int threads = std::max(1u, std::thread::hardware_concurrency());
    Generator g;

    StructureConfig sconf;
    getStructureConfig(styp, MC_VERSION, &sconf);

    printf("Preparing seed bases...\n");

    // https://github.com/Cubitect/cubiomes?tab=readme-ov-file#quad-witch-huts
    int err = searchAll48(&bases, &basecnt, NULL, threads,
                          low20QuadIdeal, 20, check, &sconf, NULL); // low20QuadIdeal, low20QuadClassic, low20QuadHutBarely

    if (err || !bases)
    {
        printf("Failed to generate seed bases.\n");
        exit(1);
    }
    else
    {
        printf("Found %" PRIu64 " seed bases.\n\n", basecnt);
    }

    std::mutex bestMutex;
    int bestArea = 0;

    setupGenerator(&g, MC_VERSION, 0);

    uint64_t i;
    for (i = 0; i < basecnt; ++i)
    {
        uint64_t s48 = moveStructure(bases[i] - sconf.salt, -1, -1);

        Pos pos[4];
        getStructurePos(styp, MC_VERSION, s48, -1, -1, &pos[0]);
        getStructurePos(styp, MC_VERSION, s48, -1, 0, &pos[1]);
        getStructurePos(styp, MC_VERSION, s48, 0, -1, &pos[2]);
        getStructurePos(styp, MC_VERSION, s48, 0, 0, &pos[3]);

        uint64_t high;
        for (high = 0; high < 0x10000; high++)
        {
            uint64_t seed = s48 | (high << 48);
            applySeed(&g, DIM_OVERWORLD, seed);

            if (isViableStructurePos(styp, &g, pos[0].x, pos[0].z, 0) &&
                isViableStructurePos(styp, &g, pos[1].x, pos[1].z, 0) &&
                isViableStructurePos(styp, &g, pos[2].x, pos[2].z, 0) &&
                isViableStructurePos(styp, &g, pos[3].x, pos[3].z, 0))
            {
                int swampTotal = 0;
                swampTotal += countSwampBlocks(&g, pos[0].x, pos[0].z);
                swampTotal += countSwampBlocks(&g, pos[1].x, pos[1].z);
                swampTotal += countSwampBlocks(&g, pos[2].x, pos[2].z);
                swampTotal += countSwampBlocks(&g, pos[3].x, pos[3].z);

                {
                    std::lock_guard<std::mutex> lock(bestMutex);
                    if (swampTotal >= bestArea && swampTotal > 0)
                    {
                        bestArea = swampTotal;
                        printf("[NEW BEST] seed=%" PRId64 ", swamp-blocks=%d\n", (int64_t)seed, swampTotal);
                        for (int p = 0; p < 4; ++p)
                            printf("  DesertPyramid %d: /tp @p %d, 100, %d | %d\n", p, pos[p].x, pos[p].z, countSwampBlocks(&g, pos[p].x, pos[p].z));

                        if (log)
                        {
                            log << (int64_t)seed << ", " << swampTotal << "\n";
                            for (int p = 0; p < 4; ++p)
                                log << "\tDesertPyramid #" << p + 1 << ": " << pos[p].x << ", " << pos[p].z << ",\t" << countSwampBlocks(&g, pos[p].x, pos[p].z) << "\n";
                            log.flush();
                        }
                    }
                }
            }
        }
    }

    free(bases);
    printf("Done.\n");
    return 0;
}
