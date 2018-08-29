extern "C" {
#include <libavformat\avformat.h>
}


struct VSOutput {
	AVFormatContext * format_ctx;

	// Track the last dts we output. We use it to double check that dts is
	// monotonic.
	//
	// I am not sure if it is available anywhere already. I tried
	// AVStream->info->last_dts and that is apparently not set.
	int64_t last_dts;
};