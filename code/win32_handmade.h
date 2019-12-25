#if !defined(WIN32_HANDMADE_H)

struct win32_sound_output {
  int samplesPerSecond;
  uint32 runningSampleIndex;
  int bytesPerSample;
  DWORD bufferSize;
  DWORD safetyBytes;
  real32 tSine;
};

struct win32_offscreen_buffer {
  BITMAPINFO info;
  void *memory;
  int width;
  int height;
  int pitch;
  int bytesPerPixel;
};

struct win32_window_dimensions {
  int width;
  int height;
};

struct win32_debug_time_marker {
  DWORD outputPlayCursor;
  DWORD outputWriteCursor;

  DWORD outputLocation;
  DWORD outputByteCount;

  DWORD expectedFlipPlayCursor;
  DWORD flipPlayCursor;
  DWORD flipWriteCursor;
};

struct win32_game_code {
  HMODULE gameCodeDLL;
  FILETIME lastWriteTime;
  game_update_and_render *updateAndRender;
  game_get_sound_samples *getSoundSamples;

  bool isValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer {
  HANDLE fileHandle;
  HANDLE memoryMap;
  char fileName[WIN32_STATE_FILE_NAME_COUNT];
  void *memoryBlock;
};

struct win32_state {
  void *gameMemoryBlock;
  uint64 totalSize;
  
  HANDLE recordingHandle;
  int inputRecordingIndex;

  HANDLE playbackHandle;
  int inputPlayingIndex;

  win32_replay_buffer replayBuffers[4];

  char exePath[WIN32_STATE_FILE_NAME_COUNT];
  char *onePastLastSlash;
};

#define WIN32_HANDMADE_H
#endif
