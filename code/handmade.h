#if !defined(HANDMADE_H)

/*
 * NOTE(higanbana):
 *
 * HANDMADE_INTERNAL:
 *   0 - Build for public release
 *   1 - Build for developer only
 *
 * HANDMADE_SLOW:
 *   0 - No slow code allowed!
 *   1 - Slow code welcome.
 */

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) { *(int *)0 = 0;}
#else
#define Assert(Expression)
#endif


#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline u32 SafeTruncateUInt64(u64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    u32 Result = (u32)Value;
    return (Result);
}

struct game_offscreen_buffer {
    void* Memory;
    int Width;
    int Height;
	int Pitch;
};

struct game_sound_output_buffer {
	int SamplesPerSecond;
	int SampleCount;
	i16 *Samples;
};

struct game_button_state {
	i32 HalfTransitionCount;
	b32 EndedDown;
};

struct game_controller_input {
	b32 IsConnected;
	b32 IsAnalog;
	
    f32 StickAverageX;
    f32 StickAverageY;
    
	union {
		game_button_state Buttons[12];
		struct {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;
            
			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;
            
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
            game_button_state Back;
            game_button_state Start;
            
            // NOTE(higanbana): All buttons should be added above this line
            game_button_state Terminator;
		};
	};
};

struct game_input {
	game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input *Input, int ControllerIndex) {
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return (Result);
}

struct game_memory {
	u64 PermanentStorageSize;
	void *PermanentStorage; // NOTE(higanbana): REQUIRED to be cleared to zero at startup
    
	u64 TransientStorageSize;
	void *TransientStorage;
    
	b32 IsInitialized;
};

#if HANDMADE_INTERNAL
struct debug_read_file_result {
    u32 ContentsSize;
    void *Contents;
};

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *filename);
internal void DEBUGPlatformFreeFileMemory(void *Memory);
internal b32 DEBUGPlatformWriteEntireFile(char *Filename, u32 MemorySize, void *Memory);
#endif


internal void GameUpdateAndRender(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);

struct game_state {
	int ToneHz;
	int XOffset;
	int YOffset;
};
#define HANDMADE_H
#endif
