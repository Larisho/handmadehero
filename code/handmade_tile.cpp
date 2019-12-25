inline void canonicalizeCoord(tile_map *tileMap, uint32 *tileCoord, real32 *tileRelCoord) {
  // NOTE(gab): World is assumed to be toroidal. If you step off the world,
  // you'll show up on the other side
  int32 offset = roundReal32ToInt32(*tileRelCoord / tileMap->tileSideInMeters);
  
  *tileCoord += offset;
  *tileRelCoord -= offset * tileMap->tileSideInMeters;

  assert(*tileRelCoord >= -0.5f * tileMap->tileSideInMeters);
  assert(*tileRelCoord <= 0.5f * tileMap->tileSideInMeters);
}

inline tile_map_position recanonicalizePosition(tile_map *tileMap, tile_map_position pos) {
  tile_map_position result = pos;

  canonicalizeCoord(tileMap, &result.absTileX, &result.tileRelX);
  canonicalizeCoord(tileMap, &result.absTileY, &result.tileRelY);

  return result;
}

inline tile_chunk *getTileChunk(tile_map *tileMap, uint32 tileChunkX, uint32 tileChunkY, uint32 tileChunkZ) {
  tile_chunk *tileChunk = 0;

  if (tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX &&
      tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY &&
      tileChunkZ >= 0 && tileChunkZ < tileMap->tileChunkCountZ) {
    tileChunk = &tileMap->tileChunks[(tileChunkZ * tileMap->tileChunkCountY * tileMap->tileChunkCountX) +
				     (tileChunkY * tileMap->tileChunkCountX)+
				     tileChunkX];
  }

  return tileChunk;
}

inline uint32 getTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY) {
  assert(tileChunk);
  assert(tileX < tileMap->chunkDim);
  assert(tileY < tileMap->chunkDim);
  
  return tileChunk->tiles[(tileY * tileMap->chunkDim) + tileX];
}

inline void setTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY, uint32 tileValue) {
  assert(tileChunk);
  assert(tileX < tileMap->chunkDim);
  assert(tileY < tileMap->chunkDim);
  
  tileChunk->tiles[(tileY * tileMap->chunkDim) + tileX] = tileValue;
}

inline uint32 getTileValue(tile_map *tileMap, tile_chunk *tileChunk, uint32 testTileX, uint32 testTileY) {
  uint32 result = 0;

  if (tileChunk && tileChunk->tiles) {
    result = getTileValueUnchecked(tileMap, tileChunk, testTileX, testTileY);
  }
  
  return result;
}

inline void setTileValue(tile_map *tileMap, tile_chunk *tileChunk, uint32 testTileX, uint32 testTileY, uint32 tileValue) {
  uint32 result = 0;

  if (tileChunk && tileChunk->tiles) {
    setTileValueUnchecked(tileMap, tileChunk, testTileX, testTileY, tileValue);
  }
  
}

inline tile_chunk_position getChunkPosFor(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ) {
  tile_chunk_position result;

  result.tileChunkX = absTileX >> tileMap->chunkShift;
  result.tileChunkY = absTileY >> tileMap->chunkShift;
  result.tileChunkZ = absTileZ;
  result.relTileX = absTileX & tileMap->chunkMask;
  result.relTileY = absTileY & tileMap->chunkMask;

  return result;
}

internal uint32 getTileValue(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ) {
  tile_chunk_position chunkPos = getChunkPosFor(tileMap, absTileX, absTileY, absTileZ);
  tile_chunk *tileChunk = getTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);

  return getTileValue(tileMap, tileChunk, chunkPos.relTileX, chunkPos.relTileY);
}

internal bool isTileMapPointEmpty(tile_map *tileMap, tile_map_position canPos) {
  uint32 tileChunkValue = 0;

  tileChunkValue = getTileValue(tileMap, canPos.absTileX, canPos.absTileY, canPos.absTileZ);
  
  return tileChunkValue == 1;
}

internal void setTileValue(memory_arena *arena, tile_map *tileMap,
			   uint32 absTileX, uint32 absTileY, uint32 absTileZ, uint32 tileValue) {
  tile_chunk_position chunkPos = getChunkPosFor(tileMap, absTileX, absTileY, absTileZ);
  tile_chunk *tileChunk = getTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);

  assert(tileChunk);

  if (!tileChunk->tiles) {
    uint32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
    
    tileChunk->tiles = pushArray(arena, tileCount, uint32);

    for (uint32 i = 0; i < tileCount; i++) {
      tileChunk->tiles[i] = 1;
    }
  }

  setTileValue(tileMap, tileChunk, chunkPos.relTileX, chunkPos.relTileY, tileValue);
}
