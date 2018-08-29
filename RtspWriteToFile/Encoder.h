#include <stdio.h>
#include <iostream>

extern "C"{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include  <libavcodec/avcodec.h>
#include  <libavformat/avformat.h>
#include <libavutil/imgutils.h> //for av_image_alloc only
#include <libavutil/opt.h>
}
#define OUTPUT_CODEC AV_CODEC_ID_H264
//Input pix fmt is set to BGR24
#define OUTPUT_PIX_FMT AV_PIX_FMT_YUV420P
class Encoder{
private:
	AVFormatContext *fmt_ctx;
	AVCodecContext  *codec_ctx; //a shortcut to st->codec
	AVStream        *st;
	AVFrame         *tmp_frame;
	int              pts = 0;
public:
	Encoder(int width, int height, const char* target);
	~Encoder();
	int write(AVFrame *frame);


	void video_encode_example(const char *filename, AVCodecID codec_id);
};