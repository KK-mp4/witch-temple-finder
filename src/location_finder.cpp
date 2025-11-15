#include <finder_utils.hpp>

// Extra config
constexpr unsigned int AREA_RADIUS_BLOCKS = 30000000;
constexpr unsigned int AREA_RADIUS_REGIONS = AREA_RADIUS_BLOCKS / (CHUNK_SIZE * 32); // 58593
constexpr unsigned int PRINT_PROGRESS_EVERY_REGIONS = 137327930;                     // Every ~1%

// Directions for spiral traversal in order: right, up, left, down
constexpr int dx[4] = {1, 0, -1, 0};
constexpr int dy[4] = {0, 1, 0, -1};

int run_location_finder(uint64_t seed = 0)
{
    printf("Searching through whole %llu seed...\n\n", seed);

    std::ofstream log("logs/location_finder.log", std::ios::app);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/location_finder.log'\n");
    else
        log << "\n\nstructure_type,\tworld_x,\tworld_z,\tswamp_spawn_blocks\n";

    int styp = Desert_Pyramid;
    StructureConfig sconf;
    getStructureConfig(styp, MC_VERSION, &sconf);

    Generator g;
    setupGenerator(&g, MC_VERSION, 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    uint64_t totalRegions = (2ULL * AREA_RADIUS_REGIONS + 1) * (2ULL * AREA_RADIUS_REGIONS + 1); // Total number of regions to process
    uint64_t scannedRegions = 0;

    int regionX = 0, regionZ = 0, templeType = 0, swampSpawnBlocks = 0, mostSwampSpawnBlocks = 0;

    Pos pos;
    getStructurePos(styp, MC_VERSION, seed, regionX, regionZ, &pos);

    templeType = isViableTemplePos(&g, pos.x, pos.z);

    if (templeType)
    {
        swampSpawnBlocks = countSwampSpawnBlocks(&g, pos.x, pos.z, templeType);

        if (swampSpawnBlocks > 0 && swampSpawnBlocks >= mostSwampSpawnBlocks)
        {
            mostSwampSpawnBlocks = swampSpawnBlocks;

            const char *templeTypeName = (templeType == 1 ? "DesertPyramid" : templeType == 2 ? "JungleTemple"
                                                                                              : "WitchHut");
            printf("[NEW BEST] type=%s swamp-spawn-blocks=%d -> /tp @p %d ~ %d\n", templeTypeName, mostSwampSpawnBlocks, pos.x, pos.z);

            if (log)
            {
                log << templeTypeName << ",\t" << pos.x << ",\t" << pos.z << ",\t" << mostSwampSpawnBlocks << "\n";
                log.flush();
            }
        }
    }

    scannedRegions++;

    int stepLen = 1;
    while (scannedRegions < totalRegions)
    {
        for (int direction = 0; direction < 4 && scannedRegions < totalRegions; ++direction)
        {
            for (int step = 0; step < stepLen && scannedRegions < totalRegions; ++step, ++scannedRegions)
            {
                if (scannedRegions % PRINT_PROGRESS_EVERY_REGIONS == 0)
                    printf("[PROGRESS] scanned-regions=%llu total-regions=%llu best-so-far: swamp-spawn-blocks=%d\n",
                           scannedRegions, totalRegions, mostSwampSpawnBlocks);

                regionX += dx[direction];
                regionZ += dy[direction];

                getStructurePos(styp, MC_VERSION, seed, regionX, regionZ, &pos);

                templeType = isViableTemplePos(&g, pos.x, pos.z);

                if (templeType == 0)
                    continue;

                swampSpawnBlocks = countSwampSpawnBlocks(&g, pos.x, pos.z, templeType);

                if (swampSpawnBlocks <= 0)
                    continue;

                if (swampSpawnBlocks >= mostSwampSpawnBlocks)
                {
                    mostSwampSpawnBlocks = swampSpawnBlocks;

                    const char *templeTypeName = (templeType == 1 ? "DesertPyramid" : templeType == 2 ? "JungleTemple"
                                                                                                      : "WitchHut");
                    printf("[NEW BEST] type=%s swamp-spawn-blocks=%d -> /tp @p %d ~ %d\n", templeTypeName, mostSwampSpawnBlocks, pos.x, pos.z);

                    if (log)
                    {
                        log << templeTypeName << ",\t" << pos.x << ",\t" << pos.z << ",\t" << mostSwampSpawnBlocks << "\n";
                        log.flush();
                    }
                }
            }
            // Increase step length after completing up or down movement (i.e. after every 2 directions)
            if (direction % 2 == 1)
                ++stepLen;
        }
    }

    printf("Done.\n");
    return 0;
}
