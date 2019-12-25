#if !defined(HANDMADE_H)

#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#define PI32 3.14159265359f

#if HANDMADE_SLOW
  #define assert(exp) if (!(exp)) {*(int *) 0 = 0;}
#else
  #define assert(exp)
#endif

#define arraySize(array) (sizeof(array) / sizeof((array)[0]))

#define kilobytes(value) ((value) * 1024LL)
#define megabytes(value) (kilobytes(value) * 1024LL)
#define gigabytes(value) (megabytes(value) * 1024LL)
#define terabytes(value) (gigabytes(value) * 1024LL)

inline uint32 safeTruncateUInt64(uint64 val) {
  assert(val <= 0xFFFFFFFF);
  return (uint32) val;
}

/* NOTE(gab): Services that the game provides to the platform layer.
   This may expand in the future - sound of separate thread, etc. */

inline game_controller_input *getController(game_input *input, int unsigned controllerIndex) {
  assert(controllerIndex < arraySize(input->controllers));
  return &(input->controllers[controllerIndex]);
}

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to u e

//
//
//
#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct memory_arena {
  memory_index size;
  uint8 *base;
  memory_index used;
};

struct world {
  tile_map *tileMap;
};

struct game_state {
  memory_arena worldArena;
  world *gameWorld;
  tile_map_position playerP;
};

#define pushSize(arena, type) (type *) pushSize_(arena, sizeof(type))

void *pushSize_(memory_arena *arena, memory_index size) {
  assert((arena->used + size) <= arena->size);
  void *result = arena->base + arena->used;
  arena->used += size;
  return result;
}

#define pushArray(arena, count, type) (type *) pushSize_(arena, ((count) * sizeof(type)))

#define HANDMADE_H
#endif
