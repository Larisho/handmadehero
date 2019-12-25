/*
  NOTE(gab):
  
  HANDMADE_INTERNAL:
  0 - Build for public release
  1 - Build for developer only

  HANDMADE_SLOW:
  0 - No slow code allowed!
  1 - Slow code welcome
*/

#if !defined(HANDMADE_PLATFORM_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

  typedef uint8_t uint8;
  typedef uint16_t uint16;
  typedef uint32_t uint32;
  typedef uint64_t uint64;

  typedef int8_t int8;
  typedef int16_t int16;
  typedef int32_t int32;
  typedef int64_t int64;

  typedef size_t memory_index;

  typedef float real32;
  typedef double real64;

  typedef struct thread_context {
    int placeholder;
  } thread_context;

  // TODO(gab): Services that the platform layer provides to the game
#if HANDMADE_INTERNAL
  typedef struct debug_read_file_result {
    void *contents;
    uint32 contentSize;
  } debug_read_file_result;

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *context, char * filename)
  typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *context, void *memory)
  typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(thread_context *context, char *filename, uint32 memorySize, void *memory)
  typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif


  typedef struct game_memory {
    bool isInitialized;
    uint64 permanentStorageSize;
    void *permanentStorage; // NOTE(gab): required to be initialized to 0 at startup
    uint64 transientStorageSize;
    void *transientStorage;
  
    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
  } game_memory;

  // TODO(gab): In the future, rendering _specifically_ will become a three-tiered abstraction
  typedef struct game_offscreen_buffer {
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
  } game_offscreen_buffer;

  typedef struct game_sound_output_buffer {
    int samplesPerSecond;
    int sampleCount;
    int16 *samples;
  } game_sound_output_buffer;


  typedef struct game_button_state {
    int halfTransitionCount;
    bool endedDown;
  } game_button_state;

  typedef struct game_controller_input {
    bool isConnected;
    bool isAnalog;
    real32 stickAverageX;
    real32 stickAverageY;
  
    union {
      game_button_state buttons[12];
      struct {
	game_button_state moveUp;
	game_button_state moveDown;
	game_button_state moveLeft;
	game_button_state moveRight;
      
	game_button_state actionUp;
	game_button_state actionDown;
	game_button_state actionLeft;
	game_button_state actionRight;
      
	game_button_state leftShoulder;
	game_button_state rightShoulder;

	game_button_state back;
	game_button_state start;

	//

	game_button_state terminator;
      };
    };
  } game_controller_input;

  typedef struct game_input {
    game_button_state mouseButtons[5];
    int32 mouseX, mouseY, mouseZ;

    real32 dtForFrame;
  
    game_controller_input controllers[5];
  } game_input;

  // NOTE(gab): In the future, rendering, specifically, will become a three tiered abstraction
#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *context, game_memory *memory, game_offscreen_buffer *buffer, game_input *input)
  typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
  GAME_UPDATE_AND_RENDER(gameUpdateAndRenderStub);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *context, game_memory *memory, game_sound_output_buffer *soundBuffer)
  typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
  GAME_GET_SOUND_SAMPLES(gameGetSoundSamplesStub);

#ifdef __cplusplus
}
#endif

#define HANDMADE_PLATFORM_H
#endif
