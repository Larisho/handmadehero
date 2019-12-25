#include "handmade.h"
#include "handmade_random.h"
#include "handmade_tile.cpp"

internal void gameOutputSound(game_state *gameState, game_sound_output_buffer *buffer, int toneHz) {
  int16 toneVolume = 3000;
  int wavePeriod = buffer->samplesPerSecond / toneHz;

  int16 *sampleOut = buffer->samples;
  for (int sampleIndex = 0; sampleIndex < buffer->sampleCount; sampleIndex++) {
#if 0
    real32 sineValue = sinf(gameState->tSine);
    int16 sampleValue = (int16) (sineValue * toneVolume);
#else
    int16 sampleValue = 0;
#endif
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
#if 0    
    gameState->tSine += 2.0f * PI32 * 1.0f / (real32) wavePeriod;

    if (gameState->tSine > 2.0f * PI32) {
      gameState->tSine -= 2.0f * PI32;
    }
#endif
  }
}

// internal void renderWeirdGradient(game_offscreen_buffer *buffer, int blueOffset, int greenOffset) {
//   uint8 *row = (uint8 *)buffer->memory;
//   for (int y = 0; y < buffer->height; y++) {

//     uint32 *pixel = (uint32 *)row;
//     for (int x = 0; x < buffer->width; x++) {
//       uint8 b = (uint8)(x + blueOffset);
//       uint8 g = (uint8)(y + greenOffset);
			
//       *pixel++ = (g << 8) | b;
//     }

//     row += buffer->pitch;
//   }
// }

internal void drawRectangle(game_offscreen_buffer *buffer, real32 realMinX, real32 realMinY, real32 realMaxX, real32 realMaxY, real32 r, real32 g, real32 b) {
  int32 minX = roundReal32ToInt32(realMinX);
  int32 minY = roundReal32ToInt32(realMinY);
  int32 maxX = roundReal32ToInt32(realMaxX);
  int32 maxY = roundReal32ToInt32(realMaxY);

  if (minX < 0)
    minX = 0;
  if (minY < 0)
    minY = 0;
  if (maxX > buffer->width)
    maxX = buffer->width;
  if (maxY > buffer->height)
    maxY = buffer->height;
  
  uint8 *endOfBuffer = (uint8 *) buffer->memory + buffer->pitch * buffer->height;

  // BIT PATTERN: AA RR GG BB
  uint32 colour = (roundReal32ToUint32(r * 255.0f) << 16) | (roundReal32ToUint32(g * 255.0f) << 8) | (roundReal32ToUint32(b * 255.0f) << 0);
  uint8 *row = ((uint8 *) buffer->memory + (minX * buffer->bytesPerPixel) + (minY * buffer->pitch));
  
  for (int y = minY; y < maxY; y++) {
    uint32 *pixel = (uint32 *) row;
    
    for (int x = minX; x < maxX; x++) {
      *pixel++ = colour;  
    }
    row += buffer->pitch;
  }
}

internal void initializeArena(memory_arena *arena, memory_index size, uint8 *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

uint32 randomIndex = 0;

extern "C" GAME_UPDATE_AND_RENDER(gameUpdateAndRender) {
  assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) == arraySize(input->controllers[0].buttons));
  assert(sizeof(game_state) <= memory->permanentStorageSize);
  
  game_state *gameState = (game_state *) memory->permanentStorage;

  real32 playerHeight = 1.4f;
  real32 playerWidth = 0.75f * playerHeight;

  if (!memory->isInitialized) {
    // TODO(gab): This may be more appropriate to do in the platform layer
    memory->isInitialized = true;

    gameState->playerP.absTileX = 1;
    gameState->playerP.absTileY = 4;
    gameState->playerP.tileRelX = 0.0f;
    gameState->playerP.tileRelY = 0.0f;

    initializeArena(&gameState->worldArena, memory->permanentStorageSize - sizeof(game_state),
		    (uint8 *) memory->permanentStorage + sizeof(game_state));

    gameState->gameWorld = pushSize(&gameState->worldArena, world);
    world *gameWorld = gameState->gameWorld;
    gameWorld->tileMap = pushSize(&gameState->worldArena, tile_map);

    tile_map *tileMap = gameWorld->tileMap;

    // NOTE(gab): Here, we're setting our world to use 256x256 tile chunks
    tileMap->chunkShift = 4;
    tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
    tileMap->chunkDim = (1 << tileMap->chunkShift);

    tileMap->tileChunkCountX = 128;
    tileMap->tileChunkCountY = 128;
    tileMap->tileChunkCountZ = 2;
    
    tileMap->tileChunks = pushArray(&gameState->worldArena,
				    tileMap->tileChunkCountX * tileMap->tileChunkCountY * tileMap->tileChunkCountZ,
				    tile_chunk);

    tileMap->tileSideInMeters = 1.4f;

    uint32 tilesPerWidth = 17;
    uint32 tilesPerHeight = 9;

    uint32 screenY = 0;
    uint32 screenX = 0;

    uint32 absTileZ = 0;

    bool doorRight = false;
    bool doorLeft = false;
    bool doorTop = false;
    bool doorBottom = false;
    bool doorUp = false;
    bool doorDown = false;

    for (uint32 screenIndex = 0; screenIndex < 100; screenIndex++) {
      
      // TODO(gab): create random number generator
      uint32 randomChoice;
      if (doorUp || doorDown)
	randomChoice = randos[randomIndex++] % 2;
      else
	randomChoice = randos[randomIndex++] % 3;

      if (randomChoice == 2) {
	if (absTileZ == 0)
	  doorUp = true;
	else
	  doorDown = true;
      }
      else if (randomChoice == 0) {
	doorRight = true;
      } else {
	doorTop = true;
      }
      
      for (uint32 tileY = 0; tileY < tilesPerHeight; tileY++) {
	for (uint32 tileX = 0; tileX < tilesPerWidth; tileX++) {
	  uint32 absTileX = (screenX * tilesPerWidth) + tileX;
	  uint32 absTileY = (screenY * tilesPerHeight) + tileY;

	  uint32 tileValue = 1;

	  if (tileX == 0 && (!doorLeft || tileY != (tilesPerHeight / 2))) {
	    tileValue = 2;
	  }

	  if (tileX == (tilesPerWidth - 1) && (!doorRight || tileY != (tilesPerHeight / 2))) {
	      tileValue = 2;
	  }

	  if (tileY == 0 && (!doorBottom || tileX != (tilesPerWidth / 2))) {
	      tileValue = 2;
	  }
	  
	  if (tileY == (tilesPerHeight - 1) && (!doorTop || tileX != (tilesPerWidth / 2))) {
	    tileValue = 2;
	  }

	  if (tileX == 10 && tileY == 6) {
	    if (doorUp) {
	      tileValue = 3;
	    }

	    if (doorDown) {
	      tileValue = 4;
	    }
	  }
	    
	  setTileValue(&gameState->worldArena, gameWorld->tileMap, absTileX, absTileY, absTileZ, tileValue);
	}
      }

      doorLeft = doorRight;
      doorBottom = doorTop;

      if (doorUp) {
	doorUp = false;
	doorDown = true;
      } else if (doorDown) {
	doorUp = true;
	doorDown = false;
      } else {
	doorUp = false;
	doorDown = false;
      }
      
      doorRight = false;
      doorTop = false;

      if (randomChoice == 2) {
	if (absTileZ == 0)
	  absTileZ = 1;
	else
	  absTileZ = 0;
      }
      else if (randomChoice == 1)
	screenX += 1;
      else
	screenY += 1;
    }
  }

  world *gameWorld = gameState->gameWorld;
  tile_map *tileMap = gameWorld->tileMap;

  int32 tileSideInPixels = 60;
  real32 metersToPixels = ((real32) tileSideInPixels) / ((real32) tileMap->tileSideInMeters);

  real32 lowerLeftX = - (real32) tileSideInPixels / 2;
  real32 lowerLeftY = (real32) buffer->height;
  
  for (int i = 0; i < arraySize(input->controllers); i++) {
    game_controller_input *controller = getController(input, i);
    if (controller->isAnalog) {
    
    }
    else {
      real32 dPlayerX = 0.0f;
      real32 dPlayerY = 0.0f;

      if (controller->moveUp.endedDown) {
	dPlayerY = 1.0f;
      }
      if (controller->moveDown.endedDown) {
	dPlayerY = -1.0f;
      }
      if (controller->moveLeft.endedDown) {
	dPlayerX = -1.0f;
      }
      if (controller->moveRight.endedDown) {
	dPlayerX = 1.0f;
      }

      real32 playerSpeed = 2.0f;

      if (controller->actionUp.endedDown)
	playerSpeed = 10.0f;

      dPlayerX *= playerSpeed;
      dPlayerY *= playerSpeed;

      tile_map_position playerPos = gameState->playerP;
      playerPos.tileRelX += input->dtForFrame * dPlayerX;
      playerPos.tileRelY += input->dtForFrame * dPlayerY;
      
      playerPos = recanonicalizePosition(tileMap, playerPos);

      tile_map_position playerPosLeft = playerPos;
      tile_map_position playerPosRight = playerPos;

      playerPosLeft.tileRelX -= (0.5f * playerWidth);
      playerPosLeft = recanonicalizePosition(tileMap, playerPosLeft);
      playerPosRight.tileRelX += (0.5f * playerWidth);
      playerPosRight = recanonicalizePosition(tileMap, playerPosRight);

      if (isTileMapPointEmpty(tileMap, playerPos) &&
	  isTileMapPointEmpty(tileMap, playerPosLeft) &&
	  isTileMapPointEmpty(tileMap, playerPosRight)) {
	gameState->playerP = playerPos;
      }
    }
  }
  
  drawRectangle(buffer, 0.0f, 0.0f, (real32) buffer->width, (real32) buffer->height, 1.0f, 0.0f, 1.0f);

  real32 screenCenterX = 0.5f * (real32) buffer->width;
  real32 screenCenterY = 0.5f * (real32) buffer->height;

  for (int32 i = -10; i < 10; ++i) {
    for (int32 j = -20; j < 20; ++j) {
      uint32 column = gameState->playerP.absTileX + j;
      uint32 row = gameState->playerP.absTileY + i;
      
      uint32 tileID = getTileValue(tileMap, column, row, gameState->playerP.absTileZ);

      if (tileID > 0) {
	
	real32 grey = 0.5f;
      
	if (tileID == 2)
	  grey = 1.0f;

	if (row == gameState->playerP.absTileY && column == gameState->playerP.absTileX)
	  grey = 0.0f;

	if (tileID > 2)
	  grey = 0.25f;

	real32 centerX = screenCenterX -
	  (metersToPixels * gameState->playerP.tileRelX) +
	  ((real32) j) * tileSideInPixels;
	real32 centerY = screenCenterY +
	  (metersToPixels * gameState->playerP.tileRelY) -
	  ((real32) i) * tileSideInPixels;

	real32 minX = centerX - (0.5f * tileSideInPixels);
	real32 minY = centerY - (0.5f * tileSideInPixels);
	real32 maxX = centerX + (0.5f * tileSideInPixels);
	real32 maxY = centerY + (0.5f * tileSideInPixels);
      
	drawRectangle(buffer, minX, minY, maxX, maxY, grey, grey, grey);
      }
    }
  }

  real32 playerR = 1.0f;
  real32 playerG = 1.0f;
  real32 playerB = 0.0f;
  real32 playerLeft = screenCenterX - (0.5f * (metersToPixels * playerWidth));
  real32 playerTop = screenCenterY - (metersToPixels * playerHeight);
  
  drawRectangle(buffer, playerLeft, playerTop,
		playerLeft + (metersToPixels * playerWidth),
		playerTop + (metersToPixels * playerHeight),
		playerR, playerG, playerB);  
}

extern "C" GAME_GET_SOUND_SAMPLES(gameGetSoundSamples) {
  gameOutputSound((game_state *) memory->permanentStorage, soundBuffer, 400);
}
