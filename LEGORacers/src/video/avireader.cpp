#include "video/avireader.h"

#include <stdlib.h>
#include <string.h>

static uint32_t Fourcc(const char* p_tag)
{
	return (uint32_t) ((uint8_t) p_tag[0] | ((uint8_t) p_tag[1] << 8) | ((uint8_t) p_tag[2] << 16) |
					   ((uint8_t) p_tag[3] << 24));
}

static bool ReadU32(FILE* p_file, uint32_t* p_value)
{
	uint8_t bytes[4];
	if (fread(bytes, 1, 4, p_file) != 4) {
		return false;
	}
	*p_value = (uint32_t) (bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((uint32_t) bytes[3] << 24));
	return true;
}

AviReader::AviReader()
{
	m_file = NULL;
	m_moviEnd = 0;
	m_buffer = NULL;
	m_bufferCapacity = 0;
	m_width = 0;
	m_height = 0;
	m_videoCodec = 0;
	m_frameDurationUs = 66667;
	m_frameCount = 0;
	m_audioChannels = 0;
	m_audioSampleRate = 0;
	m_audioBitsPerSample = 0;
}

AviReader::~AviReader()
{
	Close();
}

void AviReader::Close()
{
	if (m_file) {
		fclose(m_file);
		m_file = NULL;
	}
	free(m_buffer);
	m_buffer = NULL;
	m_bufferCapacity = 0;
}

bool AviReader::Open(const char* p_path)
{
	Close();

	m_file = fopen(p_path, "rb");
	if (!m_file) {
		return false;
	}

	if (!ParseHeaders()) {
		Close();
		return false;
	}
	return true;
}

// Walks the top-level RIFF: collects avih/strh/strf stream properties, stops with
// the file positioned at the first chunk inside 'LIST movi'.
bool AviReader::ParseHeaders()
{
	uint32_t tag;
	uint32_t size;
	uint32_t format;

	if (!ReadU32(m_file, &tag) || tag != Fourcc("RIFF") || !ReadU32(m_file, &size) || !ReadU32(m_file, &format) ||
		format != Fourcc("AVI ")) {
		return false;
	}

	// Stream-header state while scanning strl lists.
	uint32_t currentStreamType = 0;

	for (;;) {
		if (!ReadU32(m_file, &tag) || !ReadU32(m_file, &size)) {
			return false;
		}

		if (tag == Fourcc("LIST")) {
			uint32_t listType;
			if (!ReadU32(m_file, &listType)) {
				return false;
			}
			if (listType == Fourcc("movi")) {
				m_moviEnd = ftell(m_file) + (long) size - 4;
				return m_videoCodec != 0;
			}
			// Descend into hdrl/strl lists.
			continue;
		}

		long next = ftell(m_file) + (long) (size + (size & 1));

		if (tag == Fourcc("avih")) {
			uint32_t values[14];
			for (int i = 0; i < 14 && (long) (i * 4) < (long) size; i++) {
				if (!ReadU32(m_file, &values[i])) {
					return false;
				}
			}
			if (values[0]) {
				m_frameDurationUs = values[0]; // dwMicroSecPerFrame
			}
			m_frameCount = values[4]; // dwTotalFrames
			m_width = values[8];
			m_height = values[9];
		}
		else if (tag == Fourcc("strh")) {
			uint32_t header[12];
			for (int i = 0; i < 12 && (long) (i * 4) < (long) size; i++) {
				if (!ReadU32(m_file, &header[i])) {
					return false;
				}
			}
			currentStreamType = header[0];
			if (currentStreamType == Fourcc("vids")) {
				m_videoCodec = header[1]; // fccHandler
				uint32_t scale = header[5];
				uint32_t rate = header[6];
				if (rate) {
					m_frameDurationUs = (uint64_t) scale * 1000000ull / rate;
				}
			}
		}
		else if (tag == Fourcc("strf")) {
			if (currentStreamType == Fourcc("vids")) {
				// BITMAPINFOHEADER: biSize, biWidth, biHeight, planes/bits, biCompression.
				uint32_t info[5];
				for (int i = 0; i < 5 && (long) (i * 4) < (long) size; i++) {
					if (!ReadU32(m_file, &info[i])) {
						return false;
					}
				}
				if (!m_width) {
					m_width = info[1];
				}
				if (!m_height) {
					m_height = info[2];
				}
				if (info[4]) {
					m_videoCodec = info[4]; // biCompression FOURCC wins over fccHandler
				}
			}
			else if (currentStreamType == Fourcc("auds")) {
				// WAVEFORMATEX: wFormatTag+nChannels, nSamplesPerSec, ...
				uint32_t words[4];
				for (int i = 0; i < 4 && (long) (i * 4) < (long) size; i++) {
					if (!ReadU32(m_file, &words[i])) {
						return false;
					}
				}
				m_audioChannels = (uint16_t) (words[0] >> 16);
				m_audioSampleRate = words[1];
				m_audioBitsPerSample = (uint16_t) (words[3] >> 16);
			}
		}

		if (fseek(m_file, next, SEEK_SET) != 0) {
			return false;
		}
	}
}

AviReader::ChunkType AviReader::ReadChunk(const uint8_t** p_data, size_t* p_size)
{
	if (!m_file) {
		return e_endOfFile;
	}

	while (ftell(m_file) < m_moviEnd) {
		uint32_t tag;
		uint32_t size;
		if (!ReadU32(m_file, &tag) || !ReadU32(m_file, &size)) {
			return e_endOfFile;
		}

		if (tag == Fourcc("LIST")) {
			// 'rec ' groupings: descend.
			uint32_t listType;
			if (!ReadU32(m_file, &listType)) {
				return e_endOfFile;
			}
			continue;
		}

		// Stream chunks are '##dc'/'##db' (video) and '##wb' (audio).
		uint32_t kind = tag >> 16;
		bool isVideo = kind == Fourcc("00dc") >> 16 || kind == Fourcc("00db") >> 16;
		bool isAudio = kind == Fourcc("01wb") >> 16;

		if (!isVideo && !isAudio) {
			if (fseek(m_file, (long) (size + (size & 1)), SEEK_CUR) != 0) {
				return e_endOfFile;
			}
			continue;
		}

		if (size > m_bufferCapacity) {
			uint8_t* grown = (uint8_t*) realloc(m_buffer, size);
			if (!grown) {
				return e_endOfFile;
			}
			m_buffer = grown;
			m_bufferCapacity = size;
		}
		if (size && fread(m_buffer, 1, size, m_file) != size) {
			return e_endOfFile;
		}
		if (size & 1) {
			fseek(m_file, 1, SEEK_CUR);
		}

		*p_data = m_buffer;
		*p_size = size;
		return isVideo ? e_video : e_audio;
	}

	return e_endOfFile;
}
