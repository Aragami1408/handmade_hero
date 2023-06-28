/* 
   TODO(casey): THIS IS NOT A FINAL PLATFORM LAYER!!!

   - Saved game locations
   - Getting a handle to our own executable file
   - Asset loading path
   - Multithreading (launch a thread)
   - Raw Input (support for multiple keyboards)
   - Sleep/timeBeginPeriod
   - ClipCursor() (for multi-monitor support)
   - Fullscreen support
   - QueryCancelAutoplay
   - WM_SETCURSOR (control cursor visibility)
   - WM_ACTIVATEAPP (for when we are not the active application)
   - Blit speed improvements (BitBlt)
   - Hardware acceleration (OpenGL or Direct3D or both??)
   - GetKeyboardLayout (for French keyboards, international WASD support)
   */
#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>

// unsigned integers
typedef uint8_t u8;     // 1-byte long unsigned integer
typedef uint16_t u16;   // 2-byte long unsigned integer
typedef uint32_t u32;   // 4-byte long unsigned integer
typedef uint64_t u64;   // 8-byte long unsigned integer

// signed integers
typedef int8_t i8;      // 1-byte long signed integer
typedef int16_t i16;    // 2-byte long signed integer
typedef int32_t i32;    // 4-byte long signed integer
typedef int64_t i64;    // 8-byte long signed integer

// floating-point types
typedef float f32;
typedef double f64;

typedef i32 b32;

#define global_variable static
#define internal static
#define local_persist static

#define Pi32 3.14159265359f

// TODO(higanbana) Implement sine ourselves
#include <math.h>

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>
#include "win32_handmade.h"

#include "handmade.cpp"

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable IDirectSoundBuffer *GlobalSecondaryBuffer;


global_variable win32_sound_output SoundOutput;


// NOTE(higanbana): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_ 

// NOTE(higanbana): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_ 

internal void Win32LoadXInput() {
	HMODULE XInputLibrary = LoadLibraryA("Xinput1_4.dll");
	if(!XInputLibrary) {
		XInputLibrary = LoadLibraryA("Xinput1_3.dll");
	}
	if(!XInputLibrary) {
		XInputLibrary = LoadLibraryA("Xinput9_1_0.dll");
	}
    
	if(XInputLibrary) {
		XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState) XInputGetState = XInputGetStateStub;
		XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState) XInputSetState = XInputSetStateStub;
	}
	else {
		XInputGetState = XInputGetStateStub;
		XInputSetState = XInputSetStateStub;
		// TODO(higanbana): Diagnostic
	}
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState) {
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, b32 IsDown) {
	Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal f32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
    f32 Result = 0;
    
    if(Value < -DeadZoneThreshold) {
        Result = (f32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if(Value > DeadZoneThreshold) {
        Result = (f32)((Value + DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    
    return (Result);
}

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController) {
    MSG Message;
    while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
        switch(Message.message) {
            case WM_QUIT: {
                GlobalRunning = true;
            } break;
            
            // More message will go here
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:{
                u32 VKCode = (u32)Message.wParam;
                bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                
                if(IsDown != WasDown) {
                    if (VKCode == 'W') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'Q') {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'E') {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_UP) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_DOWN) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_LEFT) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_RIGHT) {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE) {
                        GlobalRunning = false;
                    }
                    else if (VKCode == VK_SPACE) {
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
					else if (VKCode == VK_BACK) {
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
                }
            } break;
            default: {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
        
    }
}

// NOTE(higanbana): DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void DEBUGPlatformFreeFileMemory(void *Memory) {
    if(Memory) {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename) {
    debug_read_file_result Result = {0};
    
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize)) {
            u32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents) {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead)) {
                    // NOTE(higanbana): File read successfully
                    Result.ContentsSize = BytesRead;
                }
                else {
                    // Error: Read failed
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                    // TODO(higanbana): Logging
                }
            }
            
            CloseHandle(FileHandle);
        }
        else {
            // Error: File size evaluation failed
            // TODO(higanbana): Logging
        }
        
        
    }
    else {
        // Error: File handle creation failed
        // TODO(higanbana): Logging
    }
    return (Result);
}

internal b32 DEBUGPlatformWriteEntireFile(char *Filename, u32 MemorySize, void *Memory) {
    b32 Result = false;
    
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
            // NOTE(higanbana): File written successfully
            Result = (BytesWritten == MemorySize);
        }
        else {
            // Error: Write failed
            // TODO(higanbana): Logging
        }
        CloseHandle(FileHandle);
    }
    else {
        // Error: Handle creation failed
        // TODO(higanbana): Logging
    }
    
    return (Result);
}

internal void Win32InitDSound(HWND Window, i32 SamplesPerSecond, i32 BufferSize) {
    // NOTE(higanbana): Load the library 
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(DSoundLibrary) {
        // NOTE(higanbana): Create a DirectSound object 
        direct_sound_create *DirectSoundCreate = (direct_sound_create *) GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        IDirectSound *DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX WaveFormat = {0};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8; // 4 under current settings
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;	
            // NOTE(higanbana): Set cooperative level
            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                // NOTE(higanbana): "Create" a primary buffer
                DSBUFFERDESC BufferDescription = {0};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                IDirectSoundBuffer *PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
                    // Set up primary buffer
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                        // Note(higanbana): We have finally set the format of the primary buffer!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                }
            }
            // NOTE(higanbana): "Create" a secondary buffer
            DSBUFFERDESC BufferDescription = {0};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0))) {
                // Note(higanbana): All good, secondary buffer works as intended
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
        }
    }
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput) {
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
        // Do the work
        u8 *DestSample = (u8 *) Region1;
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
            *DestSample++ = 0;
        }
        DestSample = (u8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
            *DestSample++ = 0;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer) {
    
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        i16 *SourceSample = SourceBuffer->Samples;
        i16 *DestSample = (i16 *)Region1;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            
            ++SoundOutput->RunningSampleIndex;
        }
        
        DestSample = (i16 *)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            
            ++SoundOutput->RunningSampleIndex;
        }
        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
    
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return(Result);
}


internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height) {
    // NOTE(higanbana): Remember to VirtualFree the memory if we ever
    // call this function more than once on the same buffer!
    /*if (Buffer->Memory) {
      VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
      }*/
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    
    // NOTE(higanbana): When the biHeight field is negative, this is the clue
    // to Windows to treat this bitmap as top-down, not bottom-up, meaning
    // that the first bytes of the image are the color for the top left
    // pixel in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = (WORD) (Buffer->BytesPerPixel * 8);
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = Buffer->BytesPerPixel * (Buffer->Width * Buffer->Height);
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight) {
    // TODO(higanbana): Aspect ratio correction
    StretchDIBits(
                  DeviceContext,
                  0, 0, WindowWidth, WindowHeight, // destination rectangle (window)
                  0, 0, Buffer->Width, Buffer->Height, // source rectangle (bitmap buffer)
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;
    switch(Message) {
        case WM_SIZE:{
        } break;
        case WM_DESTROY:{
            GlobalRunning = false;
        } break;
        case WM_CLOSE:{
            GlobalRunning = false;
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:{
            Assert(!"Keyboard input came in through a non-dispatch message!!!");
        } break;
        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;
        case WM_ACTIVATEAPP:{
            
        } break;
        default:{
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return(Result);
}



int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    i64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32LoadXInput();
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";


    if (RegisterClassA(&WindowClass)) {
        HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      0, 0, Instance, 0);
        
        if(Window) {
            
            
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else 
            LPVOID BaseAddress = 0;
#endif
            
            game_input Input[2] = {};
            game_input *OldInput = &Input[0];
            game_input *NewInput = &Input[1];
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            u64 LastCycleCount = __rdtsc();
            GlobalRunning = true;
            
            HDC DeviceContext = GetDC(Window);
            Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
            
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(i16) * 2;
            SoundOutput.SecondaryBufferSize = 2 * SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            i16 *Samples = (i16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(2);
            u64 TotalStorageSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((u8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
            
            if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) {
                // We are ready to do the main loop
                
                while(GlobalRunning) {
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) {
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
                    Win32ProcessPendingMessages(NewKeyboardController);

                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
                        MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                    }
                    
                    // TODO(higanbana): Should we poll this more frequently?
                    for(DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {
						DWORD OurControllerIndex = ControllerIndex + 1;
						game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
						game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
                        XINPUT_STATE ControllerState;
                        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
							NewController->IsConnected = true;
							// TODO(higanbana): See if ControllerState.dwPacketNumber increments too rapidly
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
							NewController->IsAnalog = true;

                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->ActionDown,XINPUT_GAMEPAD_A,
                                                            &NewController->ActionDown);
                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->ActionRight,XINPUT_GAMEPAD_B,
                                                            &NewController->ActionRight);
                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->ActionLeft,XINPUT_GAMEPAD_X,
                                                            &NewController->ActionLeft);
                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->ActionUp,XINPUT_GAMEPAD_Y,
                                                            &NewController->ActionUp);
                            
                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->LeftShoulder,XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                            &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons,
                                                            &OldController->RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                            &NewController->RightShoulder);

							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Start, XINPUT_GAMEPAD_START,
															&NewController->Start);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Back, XINPUT_GAMEPAD_BACK,
															&NewController->Back);
                            
                            NewController->StickAverageX= Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f)) {
								NewController->IsAnalog = true;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
								NewController->StickAverageY = 1.0f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
								NewController->StickAverageY = -1.0f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
								NewController->StickAverageX = -1.0f;
								NewController->IsAnalog = false;
							}
							if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
								NewController->StickAverageX = 1.0f;
								NewController->IsAnalog = false;
							}
							f32 Threshold = 0.5f;
							Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0,
									&OldController->MoveRight, 1,
									&NewController->MoveRight);
							Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0,
									&OldController->MoveLeft, 1,
									&NewController->MoveLeft);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0,
									&OldController->MoveUp, 1,
									&NewController->MoveUp);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0,
									&OldController->MoveDown, 1,
									&NewController->MoveDown);
                            
                        }
                        else {
                            // NOTE(higanbana): This controller is not available
							NewController->IsConnected = false;
                        }
                    }
                    XINPUT_VIBRATION Vibration;
                    Vibration.wLeftMotorSpeed = 60000;
                    Vibration.wRightMotorSpeed = 60000;
                    XInputSetState(0, &Vibration);
                    
                    
                    // RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
                    
                    DWORD ByteToLock;
                    DWORD TargetCursor;
                    DWORD BytesToWrite;
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    b32 SoundIsValid = false;
                    // TODO(higanbana): Tighten up sound logic so that we know where we should be
                    // writing to and can anticipate the time spent in the game updates.
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
                        
                        ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
                        TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);
                        
                        if(ByteToLock > TargetCursor) {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                            BytesToWrite += TargetCursor;
                        }
                        else {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
                        
                        SoundIsValid = true;
                    }
                    game_sound_output_buffer SoundBuffer = {0}; 
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;
                    
                    game_offscreen_buffer Buffer = {0};
                    Buffer.Memory = GlobalBackbuffer.Memory;
                    Buffer.Width = GlobalBackbuffer.Width;
                    Buffer.Height = GlobalBackbuffer.Height;
                    Buffer.Pitch = GlobalBackbuffer.Pitch;
                    
                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);
                    
                    if(SoundIsValid) {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }
                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window); 
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
                    
                    ReleaseDC(Window, DeviceContext);
                    
                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);
                    u64 EndCycleCount = __rdtsc();
                    
                    // TODO(higanbana): Display the value here
                    i16 CounterElapsed = (i16) (EndCounter.QuadPart - LastCounter.QuadPart);	
                    i64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    f64 MSPerFrame = (i32)((1000*CounterElapsed) / PerfCountFrequency);
                    f64 FPS = (i32)(PerfCountFrequency / CounterElapsed);
                    f64 MegaCyclesPerFrame = (f64) (CyclesElapsed / (1000 * 1000));
#if 0
                    char Buffer[256];
                    sprintf(Buffer, "ms/frame: %.02fms / %.02fFPS, %.02fMc/f \n", -MSPerFrame, -FPS, MegaCyclesPerFrame);
                    OutputDebugStringA(Buffer);
#endif
                    LastCounter = EndCounter;
                    LastCycleCount = EndCycleCount;
                    
                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                }
            }
            else {
                // Memory allocation failed
                // TODO(higanbana): Logging
            }
            
        }
        else {
            // Window Creation failed
            // TODO(higanbana): Logging
        }
    }
    else {
        // Window Registration failed
        // TODO(higanbana): Logging
        
    }
    return(0);
}
