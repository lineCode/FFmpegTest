extern "C" {
#include <libavformat\avformat.h>
}

struct VSInput {
	AVFormatContext * format_ctx;
	int video_stream_index;
};