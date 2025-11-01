#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

int run_seed_finder(uint64_t startSeed);
int run_quad_temple_finder(uint64_t startSeed);
int run_location_finder(uint64_t startSeed);

static void print_usage(const char *prog)
{
    std::cerr << "Usage: " << prog << " <finder> [startSeed]\n";
    std::cerr << "Finders: seed  (seed finder), quad (quad temple finder), loc (location finder)\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << prog << " seed 0\n";
    std::cerr << "  " << prog << " quad 123456789\n";
}

int main(int argc, char **argv)
{
    std::string finder = "seed";
    uint64_t startSeed = 0;

    if (argc >= 2)
    {
        std::string arg1 = argv[1];
        if (arg1.rfind("--finder=", 0) == 0)
        {
            finder = arg1.substr(strlen("--finder="));
        }
        else
        {
            finder = arg1;
        }
    }

    if (argc >= 3)
    {
        char *endptr = nullptr;
        startSeed = (uint64_t)strtoull(argv[2], &endptr, 10);
        if (endptr && *endptr != '\0')
        {
            std::cerr << "Invalid seed: " << argv[2] << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    if (finder == "seed" || finder == "seed_finder" || finder == "seedfinder")
    {
        return run_seed_finder(startSeed);
    }
    else if (finder == "quad" || finder == "quad_temple")
    {
        return run_quad_temple_finder(startSeed);
    }
    else if (finder == "loc" || finder == "location")
    {
        return run_location_finder(startSeed);
    }
    else
    {
        std::cerr << "Unknown finder: " << finder << "\n";
        print_usage(argv[0]);
        return 2;
    }
}
