#if !defined(WIN32_HANDMADE_H)

struct win32_window_dimension {
	int Width;
	int Height;
};

struct win32_offscreen_buffer {
    // Note(higanbana): Pixels are always 32-bit wide
    // Memory Order 0x BB GG RR xx
    // Little Endian 0x xx RR GG BB
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
	int Pitch;
    int BytesPerPixel;
};

struct win32_sound_output {
	int SamplesPerSecond;
	int BytesPerSample;
	int SecondaryBufferSize;
	u32 RunningSampleIndex;
	int LatencySampleCount;
};

#define WIN32_HANDMADE_H
#endif
