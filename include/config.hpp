#pragma once

extern "C"
{
#include "generator.h"
}

// Minecraft-related constants
inline constexpr int CHUNK_SIZE = 16;
inline constexpr int HALF_CHUNK = CHUNK_SIZE / 2;

// Cubiomes settings
inline constexpr int MC_VERSION = MC_1_5; // Minecraft JE 1.4.2 - ~1.6.3.
inline constexpr int QUERY_Y = 64;
inline constexpr int BIOME_QUERY_SCALE = 1;

// Structure piece sizes from TemplePieces.java (width, height, depth)
struct PieceSize
{
    int w, h, d;
};

inline constexpr PieceSize DESERT_PYRAMID = {21, 15, 21};
inline constexpr PieceSize JUNGLE_TEMPLE = {12, 10, 15};
inline constexpr PieceSize WITCH_HUT = {7, 5, 9};

enum TempleType
{
    TT_DESERT = 1,
    TT_JUNGLE = 2,
    TT_WITCH = 4
}; // Powers of 2 for bitmask

inline constexpr int SELECTED_TEMPLE_TYPES = TT_DESERT | TT_JUNGLE | TT_WITCH;
