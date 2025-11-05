#include <finder_utils.hpp>

extern "C"
{
#include "java_random.h"
}

// Extra config
constexpr int DISTANCE = 32;
constexpr int MIN_DISTANCE = 8;
constexpr int STRUCTURE_SALT = 14357617;
constexpr int64_t WORLD_BORDER_BLOCKS = 30000000LL;            // How far from 0,0 in blocks (world border) ±30,000,000 blocks
constexpr uint64_t PRINT_PROGRESS_EVERY_CHUNKS = 100000000ULL; // Print progress every N chunks scanned

int run_location_finder(uint64_t seed = 0)
{
    const unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());

    std::ofstream log("logs/location_finder.log", std::ios::app);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/location_finder.log'\n");
    else
        log << "\n\nseed,\tstructure_type,\tx,\tz,\tswamp_spawn_blocks\n";

    // Precompute chunk ranges for the whole world-border square
    // chunkRadius is number of chunks from 0 to border along one axis
    const int64_t chunkRadius = (WORLD_BORDER_BLOCKS + CHUNK_SIZE - 1) / CHUNK_SIZE;
    const int chunkMin = (int)(-chunkRadius);
    const int chunkMax = (int)chunkRadius;

    // Axis width in chunks
    const uint64_t chunksPerAxis = (uint64_t)chunkMax - (uint64_t)chunkMin + 1ULL;
    const uint64_t totalChunks = chunksPerAxis * chunksPerAxis; // may be many (use 64-bit)

    // Shared best result: protected by mutex
    uint64_t bestSeed = seed;
    int bestX = 0, bestZ = 0;
    int bestArea = -1;
    int bestType = 0; // 1 desert, 2 jungle, 3 witch
    std::mutex bestMutex;

    // Atomic chunk provider and counters
    std::atomic<uint64_t> nextChunkIndex(0);
    std::atomic<uint64_t> processedChunks(0);

    // Worker lambda
    auto worker = [&](unsigned int workerId)
    {
        Generator g;
        setupGenerator(&g, MC_VERSION, 0);
        // Apply the same world seed for all workers
        applySeed(&g, DIM_OVERWORLD, (int64_t)seed);

        JavaRandom javaRandom;

        while (true)
        {
            uint64_t idx = nextChunkIndex.fetch_add(1, std::memory_order_relaxed);
            if (idx >= totalChunks)
                break; // all chunks claimed

            // Map linear index -> chunkX, chunkZ
            uint64_t sx = idx % chunksPerAxis;
            uint64_t sz = idx / chunksPerAxis;
            int chunkX = chunkMin + (int)sx;
            int chunkZ = chunkMin + (int)sz;

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
            if (range <= 0)
                continue;

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

            uint64_t done = processedChunks.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done % PRINT_PROGRESS_EVERY_CHUNKS == 0)
            {
                std::lock_guard<std::mutex> lock(bestMutex);
                printf("[PROGRESS] worker=%u scanned-chunks=%llu total-chunks=%llu best-so-far: seed=%llu swamp-spawn-blocks=%d at (%d,%d)\n",
                       workerId, (unsigned long long)done, (unsigned long long)totalChunks,
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

    // Final summary
    {
        std::lock_guard<std::mutex> lock(bestMutex);
        if (bestArea > 0)
        {
            const char *typeName = (bestType == 1 ? "DesertPyramid" : bestType == 2 ? "JungleTemple"
                                                                                    : "WitchHut");
            printf("[DONE] seed=%llu best type=%s start=(%d,%d) swamp-blocks=%d\n",
                   (unsigned long long)bestSeed, typeName, bestX, bestZ, bestArea);
            if (log)
            {
                log << bestSeed << "," << typeName << "," << bestX << "," << bestZ << "," << bestArea << "\n";
                log.flush();
            }
        }
        else
        {
            printf("[DONE] seed=%llu no matching temple-containing-swamp found in scanned area.\n", (unsigned long long)seed);
        }
    }

    return 0;
}
