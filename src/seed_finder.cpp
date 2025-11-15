#include <finder_utils.hpp>

// Extra config
constexpr int AREA_RADIUS_BLOCKS = 65536;
constexpr int AREA_RADIUS_REGIONS = AREA_RADIUS_BLOCKS / (CHUNK_SIZE * 32);
constexpr unsigned int PRINT_PROGRESS_EVERY_SEEDS = 128;

int run_seed_finder(uint64_t startSeed = 0)
{
    // Main thread spawns workers and then joins (but workers run forever).
    const unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());

    std::ofstream log("logs/seed_finder.log", std::ios::app);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/seed_finder.log'\n");
    else
        log << "\n\nseed,\tstructure_type,\tworld_x,\tworld_z,\tswamp_spawn_blocks\n";

    // Shared best result: protected by mutex
    int64_t bestSeed = 0;
    int bestWorldX = 0, bestWorldZ = 0, mostSwampSpawnBlocks = -1;
    int bestType = 0; // 1 desert, 2 jungle, 3 witch

    // Atomic seed provider and counters
    std::atomic<uint64_t> nextSeed(startSeed);
    std::atomic<uint64_t> processedSeeds(0);

    std::mutex bestMutex;

    // Worker lambda
    auto worker = [&](unsigned int workerId)
    {
        Generator g;
        setupGenerator(&g, MC_VERSION, 0);

        int styp = Desert_Pyramid;
        int templeType = 0, swampSpawnBlocks = 0;
        Pos pos;

        while (true)
        {
            uint64_t seed = nextSeed.fetch_add(1, std::memory_order_relaxed);

            if (seed == UINT64_MAX)
            {
                printf("Worker %u done. Reached max seed.\n", workerId);
                break;
            }

            applySeed(&g, DIM_OVERWORLD, (int64_t)seed);

            for (int regionX = -AREA_RADIUS_REGIONS; regionX <= AREA_RADIUS_REGIONS; ++regionX)
            {
                for (int regionZ = -AREA_RADIUS_REGIONS; regionZ <= AREA_RADIUS_REGIONS; ++regionZ)
                {
                    getStructurePos(styp, MC_VERSION, seed, regionX, regionZ, &pos);

                    templeType = isViableTemplePos(&g, pos.x, pos.z);

                    if (templeType == 0)
                        continue;

                    swampSpawnBlocks = countSwampSpawnBlocks(&g, pos.x, pos.z, templeType);

                    if (swampSpawnBlocks <= 0)
                        continue;

                    {
                        std::lock_guard<std::mutex> lock(bestMutex);

                        if (swampSpawnBlocks >= mostSwampSpawnBlocks)
                        {
                            mostSwampSpawnBlocks = swampSpawnBlocks;
                            bestSeed = seed;
                            bestWorldX = pos.x;
                            bestWorldZ = pos.z;
                            bestType = templeType;

                            const char *templeTypeName = (templeType == 1 ? "DesertPyramid" : templeType == 2 ? "JungleTemple"
                                                                                                              : "WitchHut");
                            printf("[NEW BEST] seed=%llu type=%s swamp-spawn-blocks=%d -> /tp @p %d ~ %d\n", (int64_t)bestSeed, templeTypeName, mostSwampSpawnBlocks, pos.x, pos.z);
                            fflush(stdout);

                            if (log)
                            {
                                log << bestSeed << ",\t" << templeTypeName << ",\t" << pos.x << ",\t" << pos.z << ",\t" << mostSwampSpawnBlocks << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }

            uint64_t done = processedSeeds.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done % PRINT_PROGRESS_EVERY_SEEDS == 0)
            {
                std::lock_guard<std::mutex> lock(bestMutex);
                printf("[PROGRESS] worker=%u processed-seeds=%llu best-so-far: seed=%llu swamp-spawn-blocks=%d at (%d,%d)\n",
                       workerId, (uint64_t)done,
                       (int64_t)bestSeed, mostSwampSpawnBlocks, bestWorldX, bestWorldZ);
                fflush(stdout);
            }
        }
    };

    // Spawn workers
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (unsigned int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(worker, t);
    }

    for (auto &th : threads)
        th.join();

    printf("Done.\n");
    return 0;
}
