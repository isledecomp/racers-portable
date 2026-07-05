#ifndef AVIREADER_H
#define AVIREADER_H

// [library:video] Minimal RIFF/AVI demuxer for the game's Indeo 5 intro movies:
// walks LIST/movi chunks sequentially and hands out raw video packets ('00dc'/'00db')
// and PCM audio packets ('01wb'). No index, no OpenDML — the game's AVIs are small,
// interleaved and played strictly forward.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

class AviReader {
public:
	enum ChunkType {
		e_endOfFile = 0,
		e_video,
		e_audio,
	};

	AviReader();
	~AviReader();

	bool Open(const char* p_path);
	void Close();

	// Reads the next audio/video chunk into an internal buffer (valid until the next
	// call). Returns e_endOfFile once the movi list is exhausted.
	ChunkType ReadChunk(const uint8_t** p_data, size_t* p_size);

	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	uint32_t GetVideoCodec() const { return m_videoCodec; } // FOURCC
	// Frame duration in microseconds (from the stream header rate/scale).
	uint64_t GetFrameDurationUs() const { return m_frameDurationUs; }
	uint32_t GetFrameCount() const { return m_frameCount; }

	bool HasAudio() const { return m_audioChannels != 0; }
	uint16_t GetAudioChannels() const { return m_audioChannels; }
	uint32_t GetAudioSampleRate() const { return m_audioSampleRate; }
	uint16_t GetAudioBitsPerSample() const { return m_audioBitsPerSample; }

private:
	bool ParseHeaders();

	FILE* m_file;
	long m_moviEnd;

	uint8_t* m_buffer;
	size_t m_bufferCapacity;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_videoCodec;
	uint64_t m_frameDurationUs;
	uint32_t m_frameCount;

	uint16_t m_audioChannels;
	uint32_t m_audioSampleRate;
	uint16_t m_audioBitsPerSample;
};

#endif // AVIREADER_H
