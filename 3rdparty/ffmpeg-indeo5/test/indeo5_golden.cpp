// Golden test: demux an AVI with AviReader, decode every video frame with the
// vendored Indeo 5 decoder and write tightly packed YUV410P rawvideo to stdout or a
// file — byte-comparable with `ffmpeg -i <avi> -f rawvideo -pix_fmt yuv410p -`.
#include "../indeo5dec.h"
#include "video/avireader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file.avi> [out.yuv]\n", argv[0]);
		return 2;
	}

	AviReader reader;
	if (!reader.Open(argv[1])) {
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 1;
	}

	uint32_t codec = reader.GetVideoCodec();
	fprintf(
		stderr,
		"%ux%u codec=%.4s frames=%u audio=%uch %uHz %ubit\n",
		reader.GetWidth(),
		reader.GetHeight(),
		(const char*) &codec,
		reader.GetFrameCount(),
		reader.GetAudioChannels(),
		reader.GetAudioSampleRate(),
		reader.GetAudioBitsPerSample()
	);

	Indeo5Decoder* decoder = Indeo5_Open((int) reader.GetWidth(), (int) reader.GetHeight());
	if (!decoder) {
		fprintf(stderr, "decoder open failed\n");
		return 1;
	}

	FILE* out = argc > 2 ? fopen(argv[2], "wb") : stdout;
	if (!out) {
		fprintf(stderr, "cannot open output\n");
		return 1;
	}

	const uint8_t* data;
	size_t size;
	unsigned frames = 0;
	unsigned errors = 0;

	AviReader::ChunkType type;
	while ((type = reader.ReadChunk(&data, &size)) != AviReader::e_endOfFile) {
		if (type != AviReader::e_video) {
			continue;
		}
		if (size == 0) {
			continue; // drop frame placeholder
		}

		Indeo5Frame frame;
		int result = Indeo5_Decode(decoder, data, size, &frame);
		if (result < 0) {
			fprintf(stderr, "frame %u: decode error %d\n", frames, result);
			errors++;
			continue;
		}
		if (result == 0) {
			continue;
		}

		for (int row = 0; row < frame.height; row++) {
			fwrite(frame.data[0] + row * frame.linesize[0], 1, frame.width, out);
		}
		int chromaWidth = (frame.width + 3) / 4;
		int chromaHeight = (frame.height + 3) / 4;
		for (int plane = 1; plane <= 2; plane++) {
			for (int row = 0; row < chromaHeight; row++) {
				fwrite(frame.data[plane] + row * frame.linesize[plane], 1, chromaWidth, out);
			}
		}
		frames++;
	}

	if (out != stdout) {
		fclose(out);
	}
	Indeo5_Close(decoder);
	fprintf(stderr, "decoded %u frames, %u errors\n", frames, errors);
	return errors ? 1 : 0;
}
