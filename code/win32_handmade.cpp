/* TODO(gab): THIS IS NOT FINAL PLATFORM CODE!!!
   
   - Saved game locations
   - Getting a handle to our own exe file
   - Asset loading path
   - Threading
   - Raw Input (Support for multiple keyboards)
   - Sleep/timeBeginPeriod
   - ClipCursor() (for multimonitor support)
   - Fullscreen support
   - WM_SETCURSOR (control cursor visibility)
   - QueryCancelAutoplay
   - WM_ACTIVEAPP (for when we are not the active app)
   - Blit speed improvements (BitBlit)
   - Hardware acceleration (OpenGL or Direct3D or BOTH???)
   - GetKeyboardLayout (for French keyboards--international WASD support)

   Just a partial list!
*/
#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
X_INPUT_GET_STATE(xInputGetStateStub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}

X_INPUT_GET_STATE(xInputSetStateStub) {
  return ERROR_DEVICE_NOT_CONNECTED;
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)

typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);

typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable x_input_get_state *xInputGetState;
global_variable x_input_set_state *xInputSetState;

global_variable bool running; // TODO(gab): This is a global for now
global_variable bool globalPause;
global_variable win32_offscreen_buffer buffer;
global_variable LPDIRECTSOUNDBUFFER secondaryBuffer;
global_variable int64 perfCountFrequency;

internal void catStrings(size_t sourceACount, char *sourceA, size_t sourceBCount, char *sourceB, size_t destCount, char *dest) {
  for (int i = 0; i < sourceACount; i++) {
    *dest++ = *sourceA++;
  }

  for (int i = 0; i < sourceBCount; i++) {
    *dest++ = *sourceB++;
  }

  *dest++ = 0;
}

internal void win32GetExeFileName(win32_state *state) {
  DWORD sizeOfFileName = GetModuleFileNameA(0, state->exePath, sizeof(state->exePath));

  state->onePastLastSlash = state->exePath;
  for (char *scan = state->exePath; *scan; scan++) {
    if (*scan == '\\') {
      state->onePastLastSlash = scan + 1;
    }
  }
}

internal int stringLength(char *string) {
  int count = 0;
  while (*string++) {
    count++;
  }

  return count;
}

internal void win32BuildExePathFileName(win32_state *state, char *filename, int destCount, char *dest) {
  catStrings(state->onePastLastSlash - state->exePath, state->exePath, stringLength(filename), filename, destCount, dest);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  HANDLE fileHandle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  debug_read_file_result result = {0};
  
  if (fileHandle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER fileSize;
    if (GetFileSizeEx(fileHandle, &fileSize)) {
      result.contents = VirtualAlloc(0, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (result.contents) {
	uint32 size = safeTruncateUInt64(fileSize.QuadPart);
	DWORD bytesRead;
	if (ReadFile(fileHandle, result.contents, size, &bytesRead, 0) && size == bytesRead) {
	  // NOTE(gab): success case
	  result.contentSize = size;
	}
	else {
	  // TODO(gab): Logging
	  DEBUGPlatformReadEntireFile(context, (char *)result.contents);
	  result.contents = 0;
	}
      }
      else {
	// TODO(gab): Logging	
      }
    }
    else {
      // TODO(gab): Logging
    }
    
    CloseHandle(fileHandle);
  }
  else {
    // TODO(gab): Logging
  }

  return result;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool result = false;
  
  HANDLE fileHandle = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (fileHandle != INVALID_HANDLE_VALUE) {
    DWORD bytesWritten;
    if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0)) {
      // NOTE(gab): Write successful
      result = (memorySize == bytesWritten);
    }
    else {
      // TODO(gab): Logging
    }

    CloseHandle(fileHandle);
  }
  else {
    // TODO(gab): Logging
  }

  return result;
}

inline FILETIME win32GetLastWriteTime(char *filename) {
  FILETIME lastWriteTime = {0};

  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesEx(filename, GetFileExInfoStandard, &data)) {
    lastWriteTime = data.ftLastWriteTime;
  }

  return lastWriteTime;
}

internal win32_game_code win32LoadGameCode(char *sourceDLLName, char *tempDLLName) {
  win32_game_code result = {0};
  result.updateAndRender = 0;
  result.getSoundSamples = 0;

  result.lastWriteTime = win32GetLastWriteTime(sourceDLLName);
  
  CopyFile(sourceDLLName, tempDLLName, false);
  result.gameCodeDLL = LoadLibrary(tempDLLName);

  if (result.gameCodeDLL) {
    result.updateAndRender = (game_update_and_render *) GetProcAddress(result.gameCodeDLL, "gameUpdateAndRender");
    result.getSoundSamples = (game_get_sound_samples *) GetProcAddress(result.gameCodeDLL, "gameGetSoundSamples");

    result.isValid = result.updateAndRender && result.getSoundSamples;
  }

  if (!result.isValid) {
    result.updateAndRender = 0;
    result.getSoundSamples = 0;
  }

  return result;
}

internal void win32UnloadGameCode(win32_game_code *gameCode) {
  if (gameCode->gameCodeDLL) {
    FreeLibrary(gameCode->gameCodeDLL);
  }

  gameCode->isValid = false;
  gameCode->updateAndRender = 0;
  gameCode->getSoundSamples = 0;
}

internal void win32LoadXInput() {
  HMODULE xInputLibrary = LoadLibrary("xinput1_4.dll");
  if (!xInputLibrary) {
    // TODO(gab): Diagnostic
    xInputLibrary = LoadLibrary("xinput9_1_0.dll");
  }
  
  if (!xInputLibrary) {
    // TODO(gab): Diagnostic
    xInputLibrary = LoadLibrary("xinput1_3.dll");
  }

  if (xInputLibrary) {
    xInputGetState = (x_input_get_state *) GetProcAddress(xInputLibrary, "XInputGetState");
    if (!xInputGetState) xInputGetState = xInputGetStateStub;
    
    xInputSetState = (x_input_set_state *) GetProcAddress(xInputLibrary, "XInputSetState");
    if (!xInputSetState) xInputSetState = (x_input_set_state *) xInputSetStateStub;

    // TODO(gab): Diagnostic
  }
  else {
    // TODO(gab): Diagnostic
  }
}

internal void win32InitDSound(HWND windowHandle, int32 samplesPerSecond, int32 bufferSize) {
  HMODULE dSoundLibrary = LoadLibrary("dsound.dll");
  
  if (dSoundLibrary) {
    direct_sound_create *directSoundCreate = (direct_sound_create *) GetProcAddress(dSoundLibrary, "DirectSoundCreate");
    
    // TODO(gab): double check that this works on XP - DirectSound 8 or 7??
    LPDIRECTSOUND directSound;
    if (directSoundCreate && SUCCEEDED(directSoundCreate(0, &directSound, 0))) {
      WAVEFORMATEX waveFormat = {};
      waveFormat.wFormatTag = WAVE_FORMAT_PCM;
      waveFormat.nChannels = 2;
      waveFormat.nSamplesPerSec = samplesPerSecond;
      waveFormat.wBitsPerSample = 16;
      waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
      waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
      waveFormat.cbSize = 0;
      
      if (SUCCEEDED(directSound->SetCooperativeLevel(windowHandle, DSSCL_PRIORITY))) {
	DSBUFFERDESC bufferDescription = {};
	bufferDescription.dwSize = sizeof(bufferDescription);
	bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
	
	LPDIRECTSOUNDBUFFER primaryBuffer;
	if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0))) {	  
	  if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
	    // NOTE(gab): We've finally set the format of the primary buffer!

	  }
	  else {
	    // TODO(gab): Diagnostic
	  } 
	}
	else {
	  // TODO(gab): Diagnostic
	}
      }
      else {
	// TODO(gab): Diagnostic
      }
      
      DSBUFFERDESC bufferDescription = {};
      bufferDescription.dwSize = sizeof(bufferDescription);
      bufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
      bufferDescription.dwBufferBytes = bufferSize;
      bufferDescription.lpwfxFormat = &waveFormat;
      
      if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &secondaryBuffer, 0))) {	  

      }
    }
    else {
      // TODO(gab): Diagnostic
    }
  }
  else {
    // TODO(gab): Diagnostic
  }
}

internal win32_window_dimensions getWindowDimensions(HWND windowHandle) {
  win32_window_dimensions windowDimensions;
  RECT rect;
  GetClientRect(windowHandle, &rect);

  windowDimensions.height = rect.bottom - rect.top;
  windowDimensions.width = rect.right - rect.left;

  return windowDimensions;
}

internal void win32ResizeDIBSection(win32_offscreen_buffer *screenBuffer, int width, int height) {
  if (screenBuffer->memory) {
    VirtualFree(screenBuffer->memory, 0, MEM_RELEASE);
  }

  screenBuffer->width = width;
  screenBuffer->height = height;

  screenBuffer->info.bmiHeader.biSize = sizeof(screenBuffer->info.bmiHeader);
  screenBuffer->info.bmiHeader.biWidth = screenBuffer->width;
  screenBuffer->info.bmiHeader.biHeight = -screenBuffer->height;
  screenBuffer->info.bmiHeader.biPlanes = 1;
  screenBuffer->info.bmiHeader.biBitCount = 32;
  screenBuffer->info.bmiHeader.biCompression = BI_RGB;
	
  // TODO(gab): Bulletproof this.
  // Maybe don't free first, but free after. Then free first if that fails.
  int bytesPerPixel = 4;
  int bitmapMemorySize = width * height * bytesPerPixel;

  screenBuffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  screenBuffer->pitch = width * bytesPerPixel;
  screenBuffer->bytesPerPixel = bytesPerPixel;
}

internal void win32DisplayBufferInWindow(HDC deviceContext, win32_offscreen_buffer *screenBuffer, int windowWidth, int windowHeight) {
  int offsetX = 10;
  int offsetY = 10;
  
  PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
  PatBlt(deviceContext, 0, offsetY + screenBuffer->height, windowWidth, windowHeight, BLACKNESS);
  PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
  PatBlt(deviceContext, offsetX + screenBuffer->width, 0, windowWidth, windowHeight, BLACKNESS);
  
  StretchDIBits(deviceContext,
		offsetX, offsetY, screenBuffer->width, screenBuffer->height,
		0, 0, screenBuffer->width, screenBuffer->height, screenBuffer->memory, &screenBuffer->info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK win32MainWindowCallback(HWND windowHandle, UINT message, WPARAM WParam, LPARAM LParam) {
  LRESULT result = 0;

  switch (message) {
  case WM_DESTROY: {
    running = false; // TODO(gab): Handle this as an error - recreate window?
  } break;

  case WM_CLOSE: {
    running = false; // TODO(gab): Handle this with a message to the user?
  } break;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYUP:
  case WM_KEYDOWN: {
    uint32 vkCode = (uint32) WParam;
    bool wasDown = ((LParam & (1 << 30)) != 0);
    bool isDown = ((LParam & (1 << 31)) == 0);

    if (wasDown != isDown) {
      if (vkCode == 'W') {
      }
      else if (vkCode == 'A') {
      }
      else if (vkCode == 'S') {
      }
      else if (vkCode == 'D') {
      }
      else if (vkCode == 'Q') {
      }
      else if (vkCode == 'E') {
      }
      else if (vkCode == VK_UP) {
      }
      else if (vkCode == VK_DOWN) {
      }
      else if (vkCode == VK_LEFT) {
      }
      else if (vkCode == VK_RIGHT) {
      }
      else if (vkCode == VK_ESCAPE) {
	OutputDebugStringA("Esacape: ");
	if (wasDown)
	  OutputDebugStringA("was down ");
	if (isDown)
	  OutputDebugStringA("is down");

	OutputDebugStringA("\n");
      }
      else if (vkCode == VK_SPACE) {
      }
    }

    bool isAltKeyDown = (LParam & (1 << 29)) != 0;
    if (vkCode == VK_F4 && isAltKeyDown) {
      running = false;
    }
  } break;

  case WM_ACTIVATEAPP: {
#if 0
    if (WParam == false)
      SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 64, LWA_ALPHA);
    else
      SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 255, LWA_ALPHA);
#endif
  } break;

  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(windowHandle, &paint);
    win32_window_dimensions windowDimensions = getWindowDimensions(windowHandle);

    win32DisplayBufferInWindow(deviceContext, &buffer, windowDimensions.width, windowDimensions.height);
    EndPaint(windowHandle, &paint);
  } break;

  default: {
    // OutputDebugStringA("default\n");
    result = DefWindowProc(windowHandle, message, WParam, LParam);
  } break;
  }

  return result;
}

internal void win32ClearSoundBuffer(win32_sound_output *soundOutput) {
  void *region1;
  DWORD region1Size;
  void *region2;
  DWORD region2Size;
  if (SUCCEEDED(secondaryBuffer->Lock(0, soundOutput->bufferSize, &region1, &region1Size, &region2, &region2Size, 0))) {
    int8 *sampleOut = (int8 *) region1;
    for (DWORD byteIndex = 0; byteIndex < region1Size; byteIndex++) {
      *sampleOut++ = 0;
    }

    sampleOut = (int8 *) region2;
    for (DWORD byteIndex = 0; byteIndex < region2Size; byteIndex++) {
      *sampleOut++ = 0;
    }
    
    secondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
}

internal void win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound_output_buffer *soundBuffer) {
  void *region1;
  DWORD region1Size;
  void *region2;
  DWORD region2Size;
  if (SUCCEEDED(secondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
    // TODO(gab): assert that region1Size is valid
    DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;

    int16 *sampleOut = (int16 *) region1;
    int16 *soundSamples = soundBuffer->samples;
    for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++) {
      *sampleOut++ = *soundSamples++;
      *sampleOut++ = *soundSamples++;
      soundOutput->runningSampleIndex++;
    }

    DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
    sampleOut = (int16 *) region2;
    for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++) {
      *sampleOut++ = *soundSamples++;
      *sampleOut++ = *soundSamples++;
      soundOutput->runningSampleIndex++;
    }

    secondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
}

internal void win32ProcessKeyboardMessage(game_button_state *newState, bool isDown) {
  if (newState->endedDown != isDown) {
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
  }
}

internal void win32ProcessXInputDigitalButton(DWORD xInputButtonState, game_button_state *oldState, DWORD buttonBit, game_button_state *newState) {
  newState->endedDown = ((xInputButtonState & buttonBit) == buttonBit);
  newState->halfTransitionCount = oldState->endedDown != newState->endedDown ? true : false;
}

internal real32 win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold) {
  real32 result = 0;
  if (value < -deadZoneThreshold) {
    result = (real32) (value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
  }
  else if (value > deadZoneThreshold){
    result = (real32)(value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
  }

  return result;
}

internal void win32GetInputFileLocation(win32_state *state, bool isInputStream, int slotIndex, int destCount, char *dest) {
  char temp[64];
  wsprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, isInputStream ? "input" : "state");
  win32BuildExePathFileName(state, temp, destCount, dest);
}

internal win32_replay_buffer *win32GetReplayBuffer(win32_state *state, unsigned int index) {
  assert(index < arraySize(state->replayBuffers));
  return &state->replayBuffers[index];
}

internal void win32BeginRecordingInput(win32_state *state, int recordingIndex) {
  win32_replay_buffer *replayBuffer = win32GetReplayBuffer(state, recordingIndex);
  
  if (replayBuffer->memoryBlock) {
    state->inputRecordingIndex = recordingIndex;

    char filename[WIN32_STATE_FILE_NAME_COUNT];
    win32GetInputFileLocation(state, true, recordingIndex, sizeof(filename), filename);
    
    state->recordingHandle = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
    LARGE_INTEGER filePosition;
    filePosition.QuadPart = state->totalSize;
    SetFilePointerEx(state->recordingHandle, filePosition, 0, FILE_BEGIN);
#endif
    CopyMemory(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
  }
}

internal void win32EndRecordingInput(win32_state *state) {
  CloseHandle(state->recordingHandle);
  state->inputRecordingIndex = 0;
}

internal void win32BeginPlaybackInput(win32_state *state, int playingIndex) {
  win32_replay_buffer *replayBuffer = win32GetReplayBuffer(state, playingIndex);
  
  if (replayBuffer->memoryBlock) {
    state->inputPlayingIndex = playingIndex;
    
    char filename[WIN32_STATE_FILE_NAME_COUNT];
    win32GetInputFileLocation(state, true, playingIndex, sizeof(filename), filename);
    
    state->playbackHandle = CreateFile(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
    LARGE_INTEGER filePosition;
    filePosition.QuadPart = state->totalSize;
    SetFilePointerEx(state->playbackHandle, filePosition, 0, FILE_BEGIN);
#endif
    CopyMemory(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
  }
}

internal void win32EndPlaybackInput(win32_state *state) {
  CloseHandle(state->playbackHandle);
  state->inputPlayingIndex = 0;
}

internal void win32RecordInput(win32_state *state, game_input *newInput) {
  DWORD bytesWritten;
  WriteFile(state->recordingHandle, newInput, sizeof(*newInput), &bytesWritten, 0);
}

internal void win32PlaybackInput(win32_state *state, game_input *newInput) {
  DWORD bytesRead = 0;
  if (ReadFile(state->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0)) {
    if (bytesRead == 0){
      int playingIndex = state->inputPlayingIndex;

      win32EndPlaybackInput(state);
      win32BeginPlaybackInput(state, playingIndex);
      ReadFile(state->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0);
    }
  }
}

internal void win32ProcessPendingMessages(win32_state *state, game_controller_input *keyboardController) {
  MSG message;
  while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT)
      running = false;

    switch (message.message) {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN: {
      uint32 vkCode = (uint32) message.wParam;
      bool wasDown = ((message.lParam & (1 << 30)) != 0);
      bool isDown = ((message.lParam & (1 << 31)) == 0);
	      
      if (wasDown != isDown) {
	if (vkCode == 'W') {
	  win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
	}
	else if (vkCode == 'A') {
	  win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
	}
	else if (vkCode == 'S') {
	  win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
	}
	else if (vkCode == 'D') {
	  win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
	}
	else if (vkCode == 'Q') {
	  win32ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
	}
	else if (vkCode == 'E') {
	  win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
	}
	else if (vkCode == VK_UP) {
	  win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
	}
	else if (vkCode == VK_DOWN) {
	  win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
	}
	else if (vkCode == VK_LEFT) {
	  win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
	}
	else if (vkCode == VK_RIGHT) {
	  win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
	}
	else if (vkCode == VK_ESCAPE) {
	  win32ProcessKeyboardMessage(&keyboardController->start, isDown);
	}
	else if (vkCode == VK_SPACE) {
	  win32ProcessKeyboardMessage(&keyboardController->back, isDown);
	}
#if HANDMADE_INTERNAL
	else if (vkCode == 'P') {
	  if (isDown)
	    globalPause = !globalPause;
	}
	else if (vkCode == 'L') {
	  if (state->inputPlayingIndex == 0) {
	    if (isDown) {
	      if (state->inputRecordingIndex == 0) {
		win32BeginRecordingInput(state, 1);
	      }
	      else {
		win32EndRecordingInput(state);
		win32BeginPlaybackInput(state, 1);
	      }
	    }
	  }
	  else {
	    win32EndPlaybackInput(state);
	  }
	}
#endif
      }
	      
      bool isAltKeyDown = (message.lParam & (1 << 29)) != 0;
      if (vkCode == VK_F4 && isAltKeyDown) {
	running = false;
      }
    } break;
    default: {
      TranslateMessage(&message);
      DispatchMessage(&message);
    } break;
    }
  }
}

inline LARGE_INTEGER win32GetWallClock() {
  LARGE_INTEGER endCounter;
  QueryPerformanceCounter(&endCounter);
  return endCounter;
}

inline real32 win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  return ((real32) (end.QuadPart - start.QuadPart) / (real32) perfCountFrequency);
}

internal void win32DrawVertical(win32_offscreen_buffer *screenBuffer, int x, int top, int bottom, uint32 colour) {
  if (top <= 0)
    top = 0;

  if (bottom > screenBuffer->height)
    bottom = screenBuffer->height;
  
  if (x >= 0 && x < screenBuffer->width) {
    uint8 *pixel = ((uint8 *) screenBuffer->memory + (x * screenBuffer->bytesPerPixel) + (top * screenBuffer->pitch));
    for (int i = top; i < bottom; i++) {
      *(uint32 *) pixel = colour;
      pixel += screenBuffer->pitch;
    }
  }
}

inline void win32DrawSoundBufferMarker(win32_offscreen_buffer *screenBuffer, DWORD cursor, win32_sound_output *soundOutput, real32 coefficient, int padX, int top, int bottom, uint32 colour) {
  int x = padX + (int) (coefficient * (real32) cursor);
  win32DrawVertical(screenBuffer, x, top, bottom, colour);
}

// internal void win32DebugSyncDisplay(win32_offscreen_buffer *screenBuffer, int lastPlayCursorCount, win32_debug_time_marker *lastMarker,
// 				    int currentMarkerIndex, win32_sound_output *soundOutput, real32 secsPerFrame) {
  
//   DWORD playColour = 0xFFFFFFFF;
//   DWORD writeColour = 0xFFFF0000;
//   DWORD expectedFlipColour = 0xFFFFFF00;
//   DWORD playWindowColour = 0xFFFF00FF;

//   int padX = 16;
//   int padY = 16;

//   int lineHeight = 64;
  
//   real32 coefficient = (real32) (screenBuffer->width - (2 * padX)) / (real32) soundOutput->bufferSize;
//   for (int i = 0; i < lastPlayCursorCount; i++) {
//     win32_debug_time_marker *marker = &lastMarker[i];

//     assert(marker->outputPlayCursor < soundOutput->bufferSize);
//     assert(marker->outputWriteCursor < soundOutput->bufferSize);
//     assert(marker->outputLocation < soundOutput->bufferSize);
//     assert(marker->flipPlayCursor < soundOutput->bufferSize);
//     assert(marker->flipWriteCursor < soundOutput->bufferSize);

//     int top = padY;
//     int bottom = padY + lineHeight;
    
//     if (i == currentMarkerIndex) {
      
//       top += lineHeight + padY;
//       bottom += lineHeight + padY;

//       int firstTop = top;

//       win32DrawSoundBufferMarker(screenBuffer, marker->outputPlayCursor, soundOutput, coefficient, padX, top, bottom, playColour);
//       win32DrawSoundBufferMarker(screenBuffer, marker->outputPlayCursor, soundOutput, coefficient, padX, top, bottom, writeColour);

//       top += lineHeight + padY;
//       bottom += lineHeight + padY;
      
//       win32DrawSoundBufferMarker(screenBuffer, marker->outputLocation, soundOutput, coefficient, padX, top, bottom, playColour);
//       win32DrawSoundBufferMarker(screenBuffer, marker->outputLocation + marker->outputByteCount, soundOutput, coefficient, padX, top, bottom, writeColour);

//       top += lineHeight + padY;
//       bottom += lineHeight + padY;

//       win32DrawSoundBufferMarker(screenBuffer, marker->expectedFlipPlayCursor, soundOutput, coefficient, padX, firstTop, bottom, expectedFlipColour);
//     }
    
//     win32DrawSoundBufferMarker(screenBuffer, marker->flipPlayCursor, soundOutput, coefficient, padX, top, bottom, playColour);
//     win32DrawSoundBufferMarker(screenBuffer, marker->flipWriteCursor + (480 * soundOutput->bytesPerSample), soundOutput, coefficient, padX, top, bottom, playWindowColour);
//     win32DrawSoundBufferMarker(screenBuffer, marker->flipWriteCursor, soundOutput, coefficient, padX, top, bottom, writeColour);
//   }
// }

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCode) {
  win32_state state = {0};
  win32GetExeFileName(&state);
  
  char dllPath[WIN32_STATE_FILE_NAME_COUNT];
  win32BuildExePathFileName(&state, "handmade.dll", sizeof(dllPath), dllPath);
  
  char dllTempPath[WIN32_STATE_FILE_NAME_COUNT];
  win32BuildExePathFileName(&state, "handmade_temp.dll", sizeof(dllTempPath), dllTempPath);

  LARGE_INTEGER perfCountFrequencyResult;
  QueryPerformanceFrequency(&perfCountFrequencyResult);
  perfCountFrequency = perfCountFrequencyResult.QuadPart;

  // NOTE(gab): setting the windows scheduler to 1ms granularity
  UINT desiredSchedulerMS = 1;
  bool sleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
  
  win32LoadXInput();
  WNDCLASS windowClass = {0};

  win32ResizeDIBSection(&buffer, 960, 540);

  windowClass.style = CS_HREDRAW | CS_VREDRAW; // | CS_OWNDC;
  windowClass.lpfnWndProc = win32MainWindowCallback;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = "HandmadeHeroWindowClass";
  
  if (RegisterClass(&windowClass)) {
    HWND windowHandle = CreateWindowEx(
				       0, // WS_EX_TOPMOST | WS_EX_LAYERED,
				       windowClass.lpszClassName,
				       "Handmade Hero",
				       WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				       CW_USEDEFAULT,
				       CW_USEDEFAULT,
				       CW_USEDEFAULT,
				       CW_USEDEFAULT,
				       0,
				       0,
				       instance,
				       0);
    if (windowHandle) {
      running = true;

      
      int monitorRefreshHz = 60;
      HDC dc = GetDC(windowHandle);
      int win32RefreshRate = GetDeviceCaps(dc, VREFRESH);
      ReleaseDC(windowHandle, dc);
      if (win32RefreshRate > 1) {
	monitorRefreshHz = win32RefreshRate;
      }
      
      real32 gameUpdateHz = (monitorRefreshHz / 2.0f);
      real32 targetElapsedPerFrame = 1.0f / gameUpdateHz;
      
      win32_sound_output soundOutput = {0};

      soundOutput.samplesPerSecond = 48000;
      soundOutput.runningSampleIndex = 0;
      soundOutput.bytesPerSample = sizeof(int16) * 2;
      soundOutput.bufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
      soundOutput.safetyBytes = (int) (((real32) soundOutput.samplesPerSecond * (real32) soundOutput.bytesPerSample) / gameUpdateHz) / 3;

      win32InitDSound(windowHandle, soundOutput.samplesPerSecond, soundOutput.bufferSize);
      win32ClearSoundBuffer(&soundOutput);
      secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      bool soundIsPlaying = true;

      // TODO(gab): Pool with bitmap VirtualAlloc
      int16 *samples = (int16 *) VirtualAlloc(0, soundOutput.bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
      LPVOID baseAddress = (LPVOID) terabytes(2);
#else
      LPVOID baseAddress = 0;
#endif
      
      game_memory gameMemory = {0};
      gameMemory.permanentStorageSize = megabytes(64);
      gameMemory.transientStorageSize = gigabytes(1);
      gameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
      gameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
      gameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
      
      state.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;

      state.gameMemoryBlock = VirtualAlloc(baseAddress, state.totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      
      gameMemory.permanentStorage = state.gameMemoryBlock;
      gameMemory.transientStorage = ((uint8 *) gameMemory.permanentStorage + gameMemory.permanentStorageSize);

      for (int i = 0; i < arraySize(state.replayBuffers); i++) {
	win32_replay_buffer *replayBuffer = &state.replayBuffers[i];
	
	win32GetInputFileLocation(&state, false, i, sizeof(replayBuffer->fileName), replayBuffer->fileName);

	replayBuffer->fileHandle = CreateFile(replayBuffer->fileName, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
	
	replayBuffer->memoryMap = CreateFileMapping(replayBuffer->fileHandle, 0, PAGE_READWRITE,
						    (state.totalSize >> 32), state.totalSize & 0xFFFFFFFF, 0);
	replayBuffer->memoryBlock = MapViewOfFile(replayBuffer->memoryMap, FILE_MAP_ALL_ACCESS, 0, 0, state.totalSize);

	if (!replayBuffer->memoryBlock) {
	  // TODO(gab): Diagnostic
	}
      }

      if (samples && gameMemory.permanentStorage && gameMemory.transientStorage) {
	game_input inputs[2] = {0};
	game_input *newInput = &inputs[0];
	game_input *oldInput = &inputs[1];
	
	DWORD debugTimeMarkersIndex = 0;
	win32_debug_time_marker debugTimeMarkers[30] = {};

	DWORD lastPlayCursor = 0;
	DWORD lastWriteCursor = 0;
	bool soundIsValid = false;
	DWORD audioLatencyBytes = 0;
	real32 audioLatencySeconds = 0;
	
	uint64 lastCycleCount = __rdtsc();
	
	win32_game_code gameCode = win32LoadGameCode(dllPath, dllTempPath);
	int loadCounter = 0;

	LARGE_INTEGER lastCounter = win32GetWallClock();
	LARGE_INTEGER flipWallClock = win32GetWallClock();
      
	while (running) {
	  newInput->dtForFrame = targetElapsedPerFrame;

	  FILETIME newWriteTime = win32GetLastWriteTime(dllPath);
	  if (CompareFileTime(&gameCode.lastWriteTime, &newWriteTime) != 0) {
	    win32UnloadGameCode(&gameCode);
	    gameCode = win32LoadGameCode(dllPath, dllTempPath);
	    loadCounter = 0;
	  }
	  
	  game_controller_input *oldKeyboardController = getController(oldInput, 0);
	  game_controller_input *newKeyboardController = getController(newInput, 0);
	  *newKeyboardController = {0};
	  newKeyboardController->isConnected = true;

	  for (int buttonIndex = 0; buttonIndex < arraySize(newKeyboardController->buttons); buttonIndex++) {
	    newKeyboardController->buttons[buttonIndex].endedDown = oldKeyboardController->buttons[buttonIndex].endedDown;
	  }
	  
	  win32ProcessPendingMessages(&state, newKeyboardController);

	  if (!globalPause) {
	    POINT mousePoint;
	    GetCursorPos(&mousePoint);
	    ScreenToClient(windowHandle, &mousePoint);
	    newInput->mouseX = mousePoint.x;
	    newInput->mouseY = mousePoint.y;
	    newInput->mouseZ = 0;
	    win32ProcessKeyboardMessage(&newInput->mouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
	    win32ProcessKeyboardMessage(&newInput->mouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
	    win32ProcessKeyboardMessage(&newInput->mouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
	    win32ProcessKeyboardMessage(&newInput->mouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
	    win32ProcessKeyboardMessage(&newInput->mouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));
	    
	    DWORD maxControllerCount = XUSER_MAX_COUNT;
	    if (maxControllerCount > arraySize(newInput->controllers - 1)) {
	      maxControllerCount = arraySize(newInput->controllers - 1);
	    }
	    // TODO(gab): Should we poll this more frequently?
	    for (DWORD i = 0; i < maxControllerCount; i++) {
	      DWORD controllerIndex = i + 1;
	      game_controller_input *oldController = getController(oldInput, controllerIndex);
	      game_controller_input *newController = getController(newInput, controllerIndex);
	  
	      XINPUT_STATE controllerState;
	      if (xInputGetState(i, &controllerState) == ERROR_SUCCESS) {
		newController->isConnected = true;
		newController->isAnalog = oldController->isAnalog;
		
		// NOTE(gab): This controller is plugged in.
		// TODO(gab): See if controllerState.dwPacketNumber increments too quickly
		XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
	      
		newController->isAnalog = true;
	      
		newController->stickAverageX = win32ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		newController->stickAverageY = win32ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

		if (newController->stickAverageX != 0.0f || newController->stickAverageY != 0.0f) {
		  newController->isAnalog = true;
		}
	      
		if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
		  newController->stickAverageY = 1.0f;
		  newController->isAnalog = false;
		}
	      
		if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
		  newController->stickAverageY = -1.0f;
		  newController->isAnalog = false;
		}

		if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
		  newController->stickAverageX = -1.0f;
		  newController->isAnalog = false;
		}
	      
		if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
		  newController->stickAverageX = 1.0f;
		  newController->isAnalog = false;
		}

		real32 threshold = 0.5f;
		win32ProcessXInputDigitalButton((newController->stickAverageX < -threshold ? 1 : 0), &oldController->moveLeft, 1, &newController->moveLeft);
		win32ProcessXInputDigitalButton((newController->stickAverageX > threshold ? 1 : 0), &oldController->moveRight, 1, &newController->moveRight);
		win32ProcessXInputDigitalButton((newController->stickAverageY > threshold ? 1 : 0), &oldController->moveUp, 1, &newController->moveUp);
		win32ProcessXInputDigitalButton((newController->stickAverageY < -threshold ? 1 : 0), &oldController->moveDown, 1, &newController->moveDown);
	    
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionDown, XINPUT_GAMEPAD_A, &newController->actionDown);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionRight, XINPUT_GAMEPAD_B, &newController->actionRight);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionLeft, XINPUT_GAMEPAD_X, &newController->actionLeft);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionUp, XINPUT_GAMEPAD_Y, &newController->actionUp);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->leftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &newController->leftShoulder);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->rightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &newController->rightShoulder);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->start, XINPUT_GAMEPAD_START, &newController->start);
		win32ProcessXInputDigitalButton(pad->wButtons, &oldController->back, XINPUT_GAMEPAD_BACK, &newController->back);
	      }
	      else {
		// NOTE(gab): This controller is not available.
		newController->isConnected = false;
	      }
	    }

	    thread_context thread;
	    
	    game_offscreen_buffer gameBuffer = {0};
	
	    gameBuffer.memory = buffer.memory;
	    gameBuffer.width = buffer.width;
	    gameBuffer.height = buffer.height;
	    gameBuffer.pitch = buffer.pitch;
	    gameBuffer.bytesPerPixel = buffer.bytesPerPixel;

	    if (state.inputRecordingIndex) {
	      win32RecordInput(&state, newInput);
	    }

	    if (state.inputPlayingIndex) {
	      win32PlaybackInput(&state, newInput);
	    }
	    
	    if (gameCode.updateAndRender) {
	      gameCode.updateAndRender(&thread, &gameMemory, &gameBuffer, newInput);
	    }

	    LARGE_INTEGER audioWallClock = win32GetWallClock();
	    real32 fromBegintoAudioSeconds = win32GetSecondsElapsed(flipWallClock, audioWallClock);
	    
	    DWORD playCursor;
	    DWORD writeCursor;
	    if ((secondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)) == DS_OK) {
	    
	      /* NOTE(gab):

		 Here is how the sound output computation works.

		 We define a safety value that is the numer of samples we think our game update loop may vary by (let's say up to 2ms).
	     
		 When we wake up to write audio, we will look and see what the play cursor position is and we will forcast ahead where
		 we think the play cursor will be on the next frame boundary.

		 We will look to see that our write cursor is before that by at least our safety value. If it is, the target fill position
		 is that frame boundary plus one frame. This gives us perfect audio sync in a card that has low enough latency.

		 If the write cusor is after that safety margin, then we assume we can never sync the audio perfectly, so we will write one
		 frame's worth of audio plus the safety margin's worth of guard samples
	      */
	    
	      if (!soundIsValid) {
		soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
		soundIsValid = true;
	      }

	      DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.bufferSize;

	      DWORD expectedSoundBytesPerFrame = (int) ((real32) (soundOutput.samplesPerSecond * soundOutput.bytesPerSample) / gameUpdateHz);

	      real32 secondsLeftUntilFlip = targetElapsedPerFrame - fromBegintoAudioSeconds;
	      // assert(secondsLeftUntilFlip > 0);
	      DWORD expectedBytesUntilFlip = (DWORD) ((secondsLeftUntilFlip / targetElapsedPerFrame) * (real32) expectedSoundBytesPerFrame);

	      DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;
	    
	      DWORD safeWriteCursor = writeCursor;
	      if (safeWriteCursor < playCursor) {
		safeWriteCursor += soundOutput.bufferSize;
	      }

	      assert(safeWriteCursor >= playCursor);
	      safeWriteCursor += soundOutput.safetyBytes;
	      bool audioCardIsLowLatent = safeWriteCursor < expectedFrameBoundaryByte;
	    
	      DWORD targetCursor = 0;
	      if (audioCardIsLowLatent) {
		targetCursor = expectedFrameBoundaryByte + expectedSoundBytesPerFrame;
	      }
	      else {
		targetCursor = (writeCursor + expectedSoundBytesPerFrame + soundOutput.safetyBytes);
	      }

	      targetCursor %= soundOutput.bufferSize;

	      DWORD bytesToWrite = 0;
	      if (byteToLock > targetCursor) {
		bytesToWrite = soundOutput.bufferSize - byteToLock;
		bytesToWrite += targetCursor;
	      }
	      else {
		bytesToWrite = targetCursor - byteToLock;
	      }

	      game_sound_output_buffer gameSoundBuffer = {0};

	      gameSoundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
	      gameSoundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
	      gameSoundBuffer.samples = samples;

	      if (gameCode.getSoundSamples) {
		gameCode.getSoundSamples(&thread, &gameMemory, &gameSoundBuffer);
	      }
	    
#if HANDMADE_INTERNAL
	      win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkersIndex];
	      marker->outputPlayCursor = playCursor;
	      marker->outputWriteCursor = writeCursor;
	      marker->outputLocation = byteToLock;
	      marker->outputByteCount = bytesToWrite;
	      marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;
	    
	      audioLatencyBytes = writeCursor < playCursor ?
						(writeCursor + soundOutput.bufferSize) - playCursor : writeCursor - playCursor;

	      audioLatencySeconds = (real32) (audioLatencyBytes / soundOutput.bytesPerSample) / (real32) soundOutput.samplesPerSecond;
#if 0
	      char textBuffer[256];
	      _snprintf_s(textBuffer, sizeof(textBuffer), "BTL:%u TC:%u BTW:%u - PC:%u WC:%u BB:%u ALS: %fs\n",
			  byteToLock, targetCursor, bytesToWrite, playCursor, writeCursor, audioLatencyBytes, audioLatencySeconds);
	      OutputDebugStringA(textBuffer);
#endif
#endif
	      win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &gameSoundBuffer);
	    }
	    else {
	      soundIsValid = false;
	    }
	  
	    LARGE_INTEGER workCounter = win32GetWallClock();
	    real32 workSecondsElapsed = win32GetSecondsElapsed(lastCounter, workCounter);

	    real32 secondsElapsedForFrame = workSecondsElapsed;
	    if (secondsElapsedForFrame < targetElapsedPerFrame) {

	      if (sleepIsGranular) {
		DWORD sleepTime = (DWORD) (1000.0f * (targetElapsedPerFrame - secondsElapsedForFrame));
	      
		if (sleepTime > 0) {
		  Sleep(sleepTime);
		}
	      }

	      if (win32GetSecondsElapsed(lastCounter, win32GetWallClock()) < targetElapsedPerFrame) {
		// TODO(gab): LOG MISS HERE
	      }
	      while (secondsElapsedForFrame < targetElapsedPerFrame) {
		secondsElapsedForFrame = win32GetSecondsElapsed(lastCounter, win32GetWallClock());
	      }
	    }
	    else {
	      // TODO(gab): MISSED FRAME RATE!
	    }

	    lastCounter = win32GetWallClock();
	    
	    HDC deviceContext = GetDC(windowHandle);
	    win32_window_dimensions windowDimensions = getWindowDimensions(windowHandle);
#if HANDMADE_INTERNAL
	    // win32DebugSyncDisplay(&buffer, arraySize(debugTimeMarkers), debugTimeMarkers, debugTimeMarkersIndex - 1, &soundOutput, targetElapsedPerFrame);
#endif

	    win32DisplayBufferInWindow(deviceContext, &buffer, windowDimensions.width, windowDimensions.height);
	    ReleaseDC(windowHandle, deviceContext);

	    flipWallClock = win32GetWallClock();
	  
#if HANDMADE_INTERNAL
	    {
	      DWORD tempPlayCursor;
	      DWORD tempWriteCursor;
	      if ((secondaryBuffer->GetCurrentPosition(&tempPlayCursor, &tempWriteCursor)) == DS_OK){
		assert(debugTimeMarkersIndex < arraySize(debugTimeMarkers));
		win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkersIndex];

		marker->flipPlayCursor = tempPlayCursor;
		marker->flipWriteCursor = tempWriteCursor;
	      }
	    }
#endif
	    // real32 msPerFrame = ((1000.0f * (real32) counterElapsed) / (real32) perfCountFrequency);
	    // real32 fps = ((real32) perfCountFrequency / (real32) counterElapsed);
	    // real32 megaCyclesPerFrame = ((real32) cyclesElapsed / (1000.0f * 1000.0f));

	    // char text[256];
	    // sprintf(text, "%fms/f, %ff/s, %fmc/f\n", msPerFrame, fps, megaCyclesPerFrame);

	    // OutputDebugStringA(text);
	  
	    game_input *temp = newInput;
	    newInput = oldInput;
	    oldInput = temp;
	  
	    uint64 endCycleCount = __rdtsc();
	    uint64 cyclesElapsed = endCycleCount - lastCycleCount;
	    lastCycleCount = endCycleCount;

#if HANDMADE_INTERNAL
	    debugTimeMarkersIndex++;
	    if (debugTimeMarkersIndex == arraySize(debugTimeMarkers))
	      debugTimeMarkersIndex = 0;
#endif
	  }

	  // win32UnloadGameCode(&gameCode);
	}
      }
      else {
	// TODO(gab): Logging
      }
    }
    else {
      // TODO(gab): Logging
    }
  }
  else {
    // TODO(gab): Logging
  }

  return 0;
}
