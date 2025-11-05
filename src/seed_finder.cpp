#include <finder_utils.hpp>

extern "C"
{
#include "java_random.h"
}

// Extra config
constexpr int DISTANCE = 32;
constexpr int MIN_DISTANCE = 8;
constexpr int STRUCTURE_SALT = 14357617;
constexpr int64_t AREA_RADIUS_BLOCKS = 65536; // from 0, 0 in blocks

int run_seed_finder(uint64_t startSeed = 0)
{
    // Main thread spawns workers and then joins (but workers run forever).
    const unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    // const unsigned int numThreads = 4;

    std::ofstream log("logs/seed_finder.log", std::ios::app);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/seed_finder.log'\n");
    else
        log << "\n\nseed,\tstructure_type,\tx,\tz,\tswamp_spawn_blocks\n";

    // Precompute chunk ranges
    const int chunkRadius = (int)((AREA_RADIUS_BLOCKS + CHUNK_SIZE - 1) / CHUNK_SIZE);
    const int chunkMin = -chunkRadius;
    const int chunkMax = chunkRadius;

    // Shared best result: protected by mutex
    uint64_t bestSeed = 0;
    int bestX = 0, bestZ = 0;
    int bestArea = -1;
    int bestType = 0; // 1 desert, 2 jungle, 3 witch

    const uint64_t printProgressEvery = 128; // Prints progress every N seeds

    // Atomic seed provider and counters
    std::atomic<uint64_t> nextSeed(startSeed);
    std::atomic<uint64_t> processedSeeds(0);

    std::mutex bestMutex;

    // Worker lambda
    auto worker = [&](unsigned int workerId)
    {
        Generator g;
        setupGenerator(&g, MC_VERSION, 0);

        JavaRandom javaRandom;

        for (;;)
        {
            uint64_t seed = nextSeed.fetch_add(1, std::memory_order_relaxed);

            // Apply seed to this thread's generator instance
            applySeed(&g, DIM_OVERWORLD, (int64_t)seed);

            for (int chunkX = chunkMin; chunkX <= chunkMax; ++chunkX)
            {
                for (int chunkZ = chunkMin; chunkZ <= chunkMax; ++chunkZ)
                {
                    int i = chunkX;
                    int j = chunkZ;
                    int cx = chunkX;
                    int cz = chunkZ;
                    if (cx < 0)
                        cx -= DISTANCE - 1;
                    if (cz < 0)
                        cz -= DISTANCE - 1;
                    int regionX = cx / DISTANCE;
                    int regionZ = cz / DISTANCE;

                    int64_t regionSeed = makeStructureSeed((int64_t)seed, regionX, regionZ, STRUCTURE_SALT);
                    javaRandom.setSeed(regionSeed);

                    int k = regionX * DISTANCE;
                    int l = regionZ * DISTANCE;

                    int range = DISTANCE - MIN_DISTANCE;
                    int offsetX = javaRandom.nextInt(range);
                    int offsetZ = javaRandom.nextInt(range);

                    if (i == (k + offsetX) && j == (l + offsetZ))
                    {
                        int startX = i * CHUNK_SIZE;
                        int startZ = j * CHUNK_SIZE;

                        int templeType = isViableTemplePos(&g, startX, startZ);

                        if (!(templeType))
                            continue;

                        int swampSpawnBlocks = countSwampSpawnBlocks(&g, startX, startZ, templeType);

                        if (swampSpawnBlocks <= 0)
                            continue;

                        {
                            std::lock_guard<std::mutex> lock(bestMutex);
                            if (swampSpawnBlocks >= bestArea)
                            {
                                bestArea = swampSpawnBlocks;
                                bestSeed = seed;
                                bestX = startX;
                                bestZ = startZ;
                                bestType = templeType;

                                const char *typeName = (bestType == 1 ? "DesertPyramid" : bestType == 2 ? "JungleTemple"
                                                                                                        : "WitchHut");
                                printf("[NEW BEST] seed=%llu type=%s start=(%d,%d) swamp-spawn-blocks=%d\n",
                                       (unsigned long long)bestSeed, typeName, bestX, bestZ, bestArea);
                                fflush(stdout);

                                if (log)
                                {
                                    log << bestSeed << ",\t" << typeName << ",\t" << bestX << ",\t" << bestZ << ",\t" << bestArea << "\n";
                                    log.flush();
                                }
                            }
                        }
                    }
                }
            }

            uint64_t done = processedSeeds.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done % printProgressEvery == 0)
            {
                std::lock_guard<std::mutex> lock(bestMutex);
                printf("[PROGRESS] worker=%u processed-seeds=%llu best-so-far: seed=%llu swamp-spawn-blocks=%d at (%d,%d)\n",
                       workerId, (unsigned long long)done,
                       (unsigned long long)bestSeed, bestArea, bestX, bestZ);
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

    return 0;
}
