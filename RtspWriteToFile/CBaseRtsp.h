#define OUTPUT_CODEC AV_CODEC_ID_H264

#pragma once

extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libavformat\avio.h>
#include <libswscale\swscale.h>
#include <libavutil\imgutils.h>
#include <libavutil\opt.h>	
}

#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avutil.lib")
#pragma comment (lib, "swscale.lib")

#include "Main.h"

struct VSInput {
	AVFormatContext * format_ctx;
	int video_stream_index;
};

struct VSOutput {
	AVFormatContext * format_ctx;

	// Track the last dts we output. We use it to double check that dts is
	// monotonic.
	//
	// I am not sure if it is available anywhere already. I tried
	// AVStream->info->last_dts and that is apparently not set.
	int64_t last_dts;
};

class CBaseRtsp
{
protected:

	//rtsp url
	PCHAR _input_url;

	void setup(void);

	struct VSInput* open_input(const char * const, const char * const, const bool);

	struct VSOutput* open_output(const char * const, const char * const, const struct VSInput * const, const bool);

	int read_packet(const struct VSInput *, AVPacket * const, const bool);

	void destroy_input(struct VSInput * const);

	void destroy_output(struct VSOutput * const);

	char* get_ffmpeg_message(int);

public:

	CBaseRtsp(PCHAR input_url);
	virtual ~CBaseRtsp();

	virtual BOOL Start() abstract;
};