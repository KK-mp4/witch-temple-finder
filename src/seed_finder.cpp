#include "java_random.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>

extern "C"
{
#include <generator.h>
}

// Config
static const int MC_VERSION = MC_1_5;
static const int64_t AREA_RADIUS_BLOCKS = 65536; // from 0, 0 in blocks
static const int CHUNK_SIZE = 16;
static const int DISTANCE = 32;
static const int MIN_DISTANCE = 8;
static const int STRUCTURE_SALT = 14357617;
static const int QUERY_Y = 64;
static const int BIOME_QUERY_SCALE = 1;

static const int BIOME_DESERT = desert;
static const int BIOME_DESERT_HILLS = desert_hills;
static const int BIOME_JUNGLE = jungle;
static const int BIOME_JUNGLE_HILLS = jungle_hills;
static const int BIOME_SWAMPLAND = swampland;

enum TempleType
{
    TT_DESERT = 1,
    TT_JUNGLE = 2,
    TT_WITCH = 4
}; // Powers of 2 for bitmask
// static const int SELECTED_TEMPLE_TYPES = TT_DESERT | TT_JUNGLE | TT_WITCH;
// A bit faster, since best will be desert temples anyways
static const int SELECTED_TEMPLE_TYPES = TT_JUNGLE;

// Structure piece sizes from TemplePieces.java (width, height, depth)
struct PieceSize
{
    int w, h, d;
};
static const PieceSize DESERT_PYRAMID = {21, 15, 21};
static const PieceSize JUNGLE_TEMPLE = {12, 10, 15};
static const PieceSize WITCH_HUT = {7, 5, 9};

// Contains list of biomes valid for TempleStructure
static inline bool biomeIsValidForTemple(int biome)
{
    return (biome == BIOME_DESERT ||
            biome == BIOME_DESERT_HILLS ||
            biome == BIOME_JUNGLE ||
            biome == BIOME_JUNGLE_HILLS ||
            biome == BIOME_SWAMPLAND);
}

static inline bool biomeIsSwamp(int biome)
{
    return (biome == BIOME_SWAMPLAND);
}

// Computes the per-region RNG seed as Minecraft does for structures
static inline int64_t makeStructureSeed(int64_t worldSeed, int regionX, int regionZ, int salt)
{
    const int64_t A = 341873128712LL;
    const int64_t B = 132897987541LL;
    return (int64_t)regionX * A + (int64_t)regionZ * B + (int64_t)worldSeed + (int64_t)salt;
}

int run_seed_finder(uint64_t startSeed = 0)
{
    // Main thread spawns workers and then joins (but workers run forever).
    const unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());

    std::ofstream log("logs/seed_finder.log", std::ios::trunc);
    if (!log)
        fprintf(stderr, "Failed to open log file 'logs/seed_finder.log'\n");
    else
        log << "seed,\tstructure_type,\tx,\tz,\tswamp_blocks\n";

    // Precompute chunk ranges
    const int chunkRadius = (int)((AREA_RADIUS_BLOCKS + CHUNK_SIZE - 1) / CHUNK_SIZE);
    const int chunkMin = -chunkRadius;
    const int chunkMax = chunkRadius;
    const int halfChunk = CHUNK_SIZE / 2;

    // Shared best result: protected by mutex
    uint64_t bestSeed = 0;
    int bestX = 0, bestZ = 0;
    int bestArea = -1;
    int bestType = 0; // 0 desert, 1 jungle, 2 witch

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
                        int worldX = i * CHUNK_SIZE + halfChunk;
                        int worldZ = j * CHUNK_SIZE + halfChunk;
                        int biomeAtCenter = getBiomeAt(&g, BIOME_QUERY_SCALE, worldX, QUERY_Y, worldZ);

                        if (!biomeIsValidForTemple(biomeAtCenter))
                            continue;

                        PieceSize piece;
                        int type;
                        if (biomeAtCenter == BIOME_JUNGLE || biomeAtCenter == BIOME_JUNGLE_HILLS)
                        {
                            piece = JUNGLE_TEMPLE;
                            type = 1;
                        }
                        else if (biomeAtCenter == BIOME_SWAMPLAND)
                        {
                            piece = WITCH_HUT;
                            type = 2;
                        }
                        else
                        {
                            piece = DESERT_PYRAMID;
                            type = 0;
                        }

                        if (type == 0 && !(SELECTED_TEMPLE_TYPES & TT_DESERT))
                            continue;
                        if (type == 1 && !(SELECTED_TEMPLE_TYPES & TT_JUNGLE))
                            continue;
                        if (type == 2 && !(SELECTED_TEMPLE_TYPES & TT_WITCH))
                            continue;

                        int startX = i * CHUNK_SIZE;
                        int startZ = j * CHUNK_SIZE;
                        int endX = startX + piece.w - 1;
                        int endZ = startZ + piece.d - 1;

                        const int totalBlocks = piece.w * piece.d;

                        // Quick skip if we already have equal or better
                        {
                            std::lock_guard<std::mutex> lock(bestMutex);
                            if (totalBlocks <= bestArea)
                                continue;
                        }

                        int swampCount = 0;
                        int scannedBlocks = 0;
                        for (int bx = startX; bx <= endX; ++bx)
                        {
                            for (int bz = startZ; bz <= endZ; ++bz)
                            {
                                int bBiome = getBiomeAt(&g, BIOME_QUERY_SCALE, bx, QUERY_Y, bz);
                                if (biomeIsSwamp(bBiome))
                                    ++swampCount;

                                ++scannedBlocks;
                                int remainingBlocks = totalBlocks - scannedBlocks;
                                // Early exit if impossible to beat current best
                                {
                                    std::lock_guard<std::mutex> lock(bestMutex);
                                    if (swampCount + remainingBlocks <= bestArea)
                                        goto after_scan_early_exit;
                                }
                            }
                        }
                    after_scan_early_exit:

                        if (swampCount > 0)
                        {
                            bool isNewBest = false;
                            {
                                std::lock_guard<std::mutex> lock(bestMutex);
                                if (swampCount > bestArea)
                                {
                                    bestArea = swampCount;
                                    bestSeed = seed;
                                    bestX = startX;
                                    bestZ = startZ;
                                    bestType = type;
                                    isNewBest = true;

                                    const char *typeName = (bestType == 0 ? "DesertPyramid" : bestType == 1 ? "JungleTemple"
                                                                                                            : "WitchHut");
                                    printf("[NEW BEST] seed=%llu type=%s start=(%d,%d) swamp-blocks=%d\n",
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
            }

            uint64_t done = processedSeeds.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done % printProgressEvery == 0)
            {
                std::lock_guard<std::mutex> lock(bestMutex);
                printf("[PROGRESS] worker=%u processed-seeds=%llu best-so-far: seed=%llu area=%d at (%d,%d)\n",
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
