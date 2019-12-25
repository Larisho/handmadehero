#if !defined(HANDMADE_TILE_H)

struct tile_chunk_position {
  uint32 tileChunkX;
  uint32 tileChunkY;
  uint32 tileChunkZ;

  uint32 relTileX;
  uint32 relTileY;
};

struct tile_map_position {
  uint32 absTileX;
  uint32 absTileY;
  uint32 absTileZ;

  // NOTE(gab): x and y relative to the tile
  real32 tileRelX;
  real32 tileRelY;
};

struct tile_chunk {
  uint32 *tiles;
};

struct tile_map {
  uint32 chunkShift;
  uint32 chunkMask;
  uint32 chunkDim;
  
  real32 tileSideInMeters;

  // NOTE(gab): implement REAL sparseness so anywhere in world can have repr
  // without giant *[]
  uint32 tileChunkCountX;
  uint32 tileChunkCountY;
  uint32 tileChunkCountZ;
  
  tile_chunk *tileChunks;
};

#define HANDMADE_TILE_H
#endif
