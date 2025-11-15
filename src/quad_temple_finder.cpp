#include <finder_utils.hpp>

int check(uint64_t s48, void *data)
{
    const StructureConfig sconf = *(const StructureConfig *)data;
    return isQuadBase(sconf, s48 - sconf.salt, 148);
}

int run_quad_temple_finder(uint64_t startIndex = 0)
{
    int styp = Desert_Pyramid;

    std::ofstream log("logs/quad_temple_finder.log", std::ios::app);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/quad_temple_finder.log'\n");
    else
        log << "\n\nseed, total_swamp_blocks\n";

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

    const uint64_t numThreads =
        std::max<uint64_t>(1, std::min<uint64_t>((uint64_t)threads, basecnt));
    std::atomic<uint64_t> nextIndex(startIndex);
    std::atomic<uint64_t> processedBases(0);
    const uint64_t printProgressEvery = 32;

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (unsigned int tid = 0; tid < numThreads; ++tid)
    {
        workers.emplace_back([tid, numThreads, basecnt, bases, styp, &sconf, &bestMutex, &bestArea, &log, &nextIndex, &processedBases, printProgressEvery]()
                             {
            Generator g;
            setupGenerator(&g, MC_VERSION, 0);

            for (;;)
            {
                uint64_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
                if (i >= basecnt)
                    break;

                uint64_t s48 = moveStructure(bases[i] - sconf.salt, -1, -1);

                Pos pos[4];
                getStructurePos(styp, MC_VERSION, s48, -1, -1, &pos[0]);
                getStructurePos(styp, MC_VERSION, s48, -1, 0, &pos[1]);
                getStructurePos(styp, MC_VERSION, s48, 0, -1, &pos[2]);
                getStructurePos(styp, MC_VERSION, s48, 0, 0, &pos[3]);

                for (uint64_t high = 0; high < 0x10000; ++high)
                {
                    uint64_t seed = s48 | (high << 48);
                    applySeed(&g, DIM_OVERWORLD, seed);

                    int templeTypes[4];
                    for (int j = 0; j < 4; ++j)
                        templeTypes[j] = isViableTemplePos(&g, pos[j].x, pos[j].z);

                    // Continue next cycle if not all 4 spawned
                    if (!(templeTypes[0] &&
                        templeTypes[1] &&
                        templeTypes[2] &&
                        templeTypes[3]))
                    {
                        continue;
                    }

                    int swampSpawnBlocksTotal = 0;
                    for (int j = 0; j < 4; ++j)
                        swampSpawnBlocksTotal += countSwampSpawnBlocks(&g, pos[j].x, pos[j].z, templeTypes[j]);

                    if (swampSpawnBlocksTotal <= 0)
                        continue;

                    {
                        std::lock_guard<std::mutex> lock(bestMutex);
                        if (swampSpawnBlocksTotal >= bestArea)
                        {
                            bestArea = swampSpawnBlocksTotal;
                            printf("[NEW BEST] seed=%" PRId64 ", swamp-spawn-blocks=%d\n", (int64_t)seed, swampSpawnBlocksTotal);
                            for (int j = 0; j < 4; ++j)
                            {
                                const char *typeName = (templeTypes[j] == 1 ? "DesertPyramid" : templeTypes[j] == 2 ? "JungleTemple"
                                                                                                                    : "WitchHut");
                                printf("\t%s, %d: '/tp @p %d ~ %d'\n", typeName, countSwampSpawnBlocks(&g, pos[j].x, pos[j].z, templeTypes[j]), pos[j].x, pos[j].z);
                            }

                            if (log)
                            {
                                log << (int64_t)seed << ", " << swampSpawnBlocksTotal << "\n";
                                for (int j = 0; j < 4; ++j)
                                {
                                    const char *typeName = (templeTypes[j] == 1 ? "DesertPyramid" : templeTypes[j] == 2 ? "JungleTemple"
                                                                                                                        : "WitchHut");
                                    log << "\t" << typeName << " - " << countSwampSpawnBlocks(&g, pos[j].x, pos[j].z, templeTypes[j]) << ": " << pos[j].x << ", " << pos[j].z << "\n";
                                }
                                log.flush();
                            }
                        }
                    }
                }

                uint64_t done = processedBases.fetch_add(1, std::memory_order_relaxed) + 1;
                if (done % printProgressEvery == 0)
                {
                    std::lock_guard<std::mutex> lock(bestMutex);
                    printf("[PROGRESS] worker=%u processed-bases=%llu best-so-far swamp-spawn-blocks=%d\n",
                        tid, (unsigned long long)done, bestArea);
                    fflush(stdout);
                }
            } });
    }

    for (auto &th : workers)
        th.join();

    printf("Done.\n");
    return 0;
}
