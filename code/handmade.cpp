#include "handmade.h"

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset) {
    u8 *Row = (u8 *)Buffer->Memory;
    
	for (int Y = 0;Y < Buffer->Height;++Y) {
		u32* Pixel = (u32*)Row;
		for (int X = 0;X < Buffer->Width;++X) {
			u8 Red = 0;
            u8 Green = (u8)(Y+YOffset);
            u8 Blue = (u8)(X+XOffset);
            *Pixel++ = Red << 16 | Green << 8 | Blue; // << 0
		}
		Row += Buffer->Pitch;
	}
    
}

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz) {
	local_persist f32 tSine;
	i16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    
	i16 *SampleOut = SoundBuffer->Samples;
	for(DWORD SampleIndex = 0; SampleIndex < (DWORD) SoundBuffer->SampleCount; ++SampleIndex) {
		f32 SineValue = sinf(tSine);
		i16 SampleValue = (i16)(SineValue * ToneVolume);
        
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
        
		tSine += 2.0f * Pi32 * 1.0f / (f32)WavePeriod;
	}
}

internal void GameUpdateAndRender(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer) {
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons) - 1));
    debug_read_file_result FileData = DEBUGPlatformReadEntireFile((char *)__FILE__);
    if(FileData.Contents) {
        DEBUGPlatformWriteEntireFile((char *)"test.out", FileData.ContentsSize, FileData.Contents);
        DEBUGPlatformFreeFileMemory(FileData.Contents);
    }
    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
	if(!Memory->IsInitialized) {
		GameState->XOffset = 0;
		GameState->YOffset = 0;
		GameState->ToneHz = 256;
		Memory->IsInitialized = true;
	}
    
	for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) {
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if(Controller->IsAnalog) {
			// NOTE(higanbana): Use analog movement tuning
			GameState->XOffset += (int)(4.0f * Controller->StickAverageX);
			GameState->ToneHz = 256 + (int)(128.0f * (Controller->StickAverageY));
		}
		else {
			// NOTE(higanbana): Use digital movement tuning
			if(Controller->MoveLeft.EndedDown || Controller->ActionLeft.EndedDown) {
				GameState->XOffset += 10;
			}

			if(Controller->MoveRight.EndedDown || Controller->ActionRight.EndedDown) {
				GameState->XOffset -= 10;
			}
		}
	}
    
    
	GameOutputSound(SoundBuffer, GameState->ToneHz);
	RenderWeirdGradient(Buffer, GameState->XOffset, GameState->YOffset);
}
