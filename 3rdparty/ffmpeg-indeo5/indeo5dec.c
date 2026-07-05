/* Implementation of the public Indeo 5 decoder API over the vendored FFmpeg
 * decoder (ff_indeo5_decoder from indeo5.c). */
#include "indeo5dec.h"

#include "shim/avcodec.h"

#include <stdlib.h>
#include <string.h>

extern AVCodec ff_indeo5_decoder;

struct Indeo5Decoder {
	AVCodecContext m_context;
	AVFrame m_frame;
	uint8_t* m_packetBuffer;
	size_t m_packetCapacity;
};

Indeo5Decoder* Indeo5_Open(int p_maxWidth, int p_maxHeight)
{
	Indeo5Decoder* decoder = (Indeo5Decoder*) calloc(1, sizeof(Indeo5Decoder));
	if (!decoder) {
		return NULL;
	}

	decoder->m_context.codec_id = AV_CODEC_ID_INDEO5;
	decoder->m_context.pix_fmt = AV_PIX_FMT_NONE;
	/* The container dimensions seed plane allocation, like FFmpeg's demuxer layer
	 * does; the bitstream's own picture header replaces them during decode. */
	decoder->m_context.width = p_maxWidth;
	decoder->m_context.height = p_maxHeight;
	decoder->m_context.max_pixels = (int64_t) p_maxWidth * p_maxHeight * 4;
	if (decoder->m_context.max_pixels <= 0) {
		decoder->m_context.max_pixels = 1920 * 1080;
	}

	decoder->m_context.priv_data = calloc(1, (size_t) ff_indeo5_decoder.priv_data_size);
	if (!decoder->m_context.priv_data || ff_indeo5_decoder.init(&decoder->m_context) < 0) {
		free(decoder->m_context.priv_data);
		free(decoder);
		return NULL;
	}

	return decoder;
}

int Indeo5_Decode(Indeo5Decoder* p_decoder, const uint8_t* p_data, size_t p_size, Indeo5Frame* p_frame)
{
	if (!p_decoder || !p_data || p_size == 0 || p_size > INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE) {
		return AVERROR(EINVAL);
	}

	/* The bit reader may look past the payload; feed it a padded copy like FFmpeg
	 * packets guarantee. */
	size_t padded = p_size + AV_INPUT_BUFFER_PADDING_SIZE;
	if (padded > p_decoder->m_packetCapacity) {
		uint8_t* grown = (uint8_t*) realloc(p_decoder->m_packetBuffer, padded);
		if (!grown) {
			return AVERROR(ENOMEM);
		}
		p_decoder->m_packetBuffer = grown;
		p_decoder->m_packetCapacity = padded;
	}
	memcpy(p_decoder->m_packetBuffer, p_data, p_size);
	memset(p_decoder->m_packetBuffer + p_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	AVPacket packet;
	packet.data = p_decoder->m_packetBuffer;
	packet.size = (int) p_size;

	av_frame_unref(&p_decoder->m_frame);

	int gotFrame = 0;
	int result = ff_indeo5_decoder.decode(&p_decoder->m_context, &p_decoder->m_frame, &gotFrame, &packet);
	if (result < 0) {
		return result;
	}
	if (!gotFrame) {
		return 0;
	}

	if (p_frame) {
		p_frame->data[0] = p_decoder->m_frame.data[0];
		p_frame->data[1] = p_decoder->m_frame.data[1];
		p_frame->data[2] = p_decoder->m_frame.data[2];
		p_frame->linesize[0] = p_decoder->m_frame.linesize[0];
		p_frame->linesize[1] = p_decoder->m_frame.linesize[1];
		p_frame->linesize[2] = p_decoder->m_frame.linesize[2];
		p_frame->width = p_decoder->m_context.width;
		p_frame->height = p_decoder->m_context.height;
	}
	return 1;
}

void Indeo5_Close(Indeo5Decoder* p_decoder)
{
	if (!p_decoder) {
		return;
	}

	ff_indeo5_decoder.close(&p_decoder->m_context);
	av_frame_unref(&p_decoder->m_frame);
	free(p_decoder->m_context.priv_data);
	free(p_decoder->m_packetBuffer);
	free(p_decoder);
}
