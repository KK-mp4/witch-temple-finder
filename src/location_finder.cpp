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

    const uint64_t totalRegions = (2ULL * AREA_RADIUS_REGIONS + 1ULL) * (2ULL * AREA_RADIUS_REGIONS + 1ULL);

    const unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());

    std::atomic<int> mostSwampSpawnBlocks{0};
    std::mutex logMutex;   // protects file writes and printf for NEW BEST
    std::mutex printMutex; // protects periodic progress printf

    // worker lambda: each thread executes the same spiral; only handles regions where (index % numThreads) == tid
    auto worker = [&](unsigned int tid)
    {
        // thread-local generator/state
        Generator g;
        setupGenerator(&g, MC_VERSION, 0);
        applySeed(&g, DIM_OVERWORLD, seed);

        uint64_t scannedRegions = 0;

        int regionX = 0, regionZ = 0;
        Pos pos;

        // process origin (region 0)
        getStructurePos(styp, MC_VERSION, seed, regionX, regionZ, &pos);

        if ((scannedRegions % numThreads) == tid)
        {
            int templeType = isViableTemplePos(&g, pos.x, pos.z);
            if (templeType)
            {
                int swampSpawnBlocks = countSwampSpawnBlocks(&g, pos.x, pos.z, templeType);
                if (swampSpawnBlocks > 0)
                {
                    int prev = mostSwampSpawnBlocks.load(std::memory_order_relaxed);
                    if (swampSpawnBlocks >= prev)
                    {
                        // log/update under mutex to avoid interleaving file writes & prints
                        std::lock_guard<std::mutex> lk(logMutex);
                        prev = mostSwampSpawnBlocks.load();
                        if (swampSpawnBlocks >= prev)
                        {
                            mostSwampSpawnBlocks.store(swampSpawnBlocks);
                            const char *templeTypeName = (templeType == 1 ? "DesertPyramid" : templeType == 2 ? "JungleTemple"
                                                                                                              : "WitchHut");
                            printf("[NEW BEST] type=%s swamp-spawn-blocks=%d -> /tp @p %d ~ %d\n",
                                   templeTypeName, swampSpawnBlocks, pos.x, pos.z);
                            if (log)
                            {
                                log << templeTypeName << ",\t" << pos.x << ",\t" << pos.z << ",\t" << swampSpawnBlocks << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }
        }

        ++scannedRegions;

        int stepLen = 1;
        while (scannedRegions < totalRegions)
        {
            for (int direction = 0; direction < 4 && scannedRegions < totalRegions; ++direction)
            {
                for (int step = 0; step < stepLen && scannedRegions < totalRegions; ++step, ++scannedRegions)
                {
                    // only thread 0 prints progress (to avoid interleaving)
                    if (tid == 0 && (scannedRegions % PRINT_PROGRESS_EVERY_REGIONS == 0))
                    {
                        std::lock_guard<std::mutex> lk(printMutex);
                        printf("[PROGRESS] scanned-regions=%llu total-regions=%llu best-so-far: swamp-spawn-blocks=%d\n",
                               scannedRegions, totalRegions, mostSwampSpawnBlocks.load());
                    }

                    regionX += dx[direction];
                    regionZ += dy[direction];

                    // modulo partitioning: only the thread that owns this index will do the heavy work
                    if ((scannedRegions % numThreads) != tid)
                        continue;

                    getStructurePos(styp, MC_VERSION, seed, regionX, regionZ, &pos);

                    int templeType = isViableTemplePos(&g, pos.x, pos.z);
                    if (templeType == 0)
                        continue;

                    int swampSpawnBlocks = countSwampSpawnBlocks(&g, pos.x, pos.z, templeType);
                    if (swampSpawnBlocks <= 0)
                        continue;

                    int prev = mostSwampSpawnBlocks.load(std::memory_order_relaxed);
                    if (swampSpawnBlocks >= prev)
                    {
                        std::lock_guard<std::mutex> lk(logMutex);
                        prev = mostSwampSpawnBlocks.load();
                        if (swampSpawnBlocks >= prev)
                        {
                            mostSwampSpawnBlocks.store(swampSpawnBlocks);
                            const char *templeTypeName = (templeType == 1 ? "DesertPyramid" : templeType == 2 ? "JungleTemple"
                                                                                                              : "WitchHut");
                            printf("[NEW BEST] type=%s swamp-spawn-blocks=%d -> /tp @p %d ~ %d\n",
                                   templeTypeName, swampSpawnBlocks, pos.x, pos.z);
                            if (log)
                            {
                                log << templeTypeName << ",\t" << pos.x << ",\t" << pos.z << ",\t" << swampSpawnBlocks << "\n";
                                log.flush();
                            }
                        }
                    }
                }
                if (direction % 2 == 1)
                    ++stepLen;
            }
        }
    };

    // spawn threads
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (unsigned int i = 0; i < numThreads; ++i)
        threads.emplace_back(worker, i);

    for (auto &t : threads)
        t.join();

    printf("Done. best-so-far swamp-spawn-blocks=%d\n", mostSwampSpawnBlocks.load());
    return 0;
}
