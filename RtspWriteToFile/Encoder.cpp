#include "Encoder.h"

Encoder::Encoder(int width, int height, const char* target){

	int err;
	AVOutputFormat  *fmt;
	AVCodec         *codec;
	AVDictionary *fmt_opts = NULL;


//////////////////////	av_register_all();
	this->fmt_ctx = avformat_alloc_context();
	if (this->fmt_ctx == NULL) {
		//throw AVException(ENOMEM, "can not alloc av context");
	}
	//init encoding format
	fmt = av_guess_format(NULL, target, NULL);
	if (!fmt) {
		std::cout << "Could not deduce output format from file extension: using MPEG4." << std::endl;
		fmt = av_guess_format("mpeg4", NULL, NULL);
	}
	//std::cout <<fmt->long_name<<std::endl;
	//Set format header infos
	fmt_ctx->oformat = fmt;
	
	//snprintf(fmt_ctx->filename, sizeof(fmt_ctx->filename), "%s", target);
	
	//Reference for AvFormatContext options : https://ffmpeg.org/doxygen/2.8/movenc_8c_source.html
	//Set format's privater options, to be passed to avformat_write_header()
	err = av_dict_set(&fmt_opts, "movflags", "faststart", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_dict_set movflags").what() << std::endl;
	}
	//default brand is "isom", which fails on some devices
	err = av_dict_set(&fmt_opts, "brand", "mp42", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_dict_set brand").what() << std::endl;
	}
	codec = avcodec_find_encoder(OUTPUT_CODEC);
	//codec = avcodec_find_encoder(fmt->video_codec);
	if (!codec) {
		//throw AVException(1, "can't find encoder");
	}
	if (!(st = avformat_new_stream(fmt_ctx, codec))) {
		//throw AVException(1, "can't create new stream");
	}
	//set stream time_base
	/* frames per second FIXME use input fps? */
	st->time_base = { 1, 25 };

	//Set codec_ctx to stream's codec structure
	codec_ctx = st->codec;

	/* put sample parameters */
	codec_ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
	codec_ctx->width = width;
	codec_ctx->height = height;
	codec_ctx->time_base = st->time_base;
	codec_ctx->pix_fmt = OUTPUT_PIX_FMT;
	/* Apparently it's in the example in master but does not work in V11
	if (o_format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	*/
	//H.264 specific options
	codec_ctx->gop_size = 25;
	codec_ctx->level = 31;
	err = av_opt_set(codec_ctx->priv_data, "crf", "12", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_opt_set crf").what() << std::endl;
	}
	err = av_opt_set(codec_ctx->priv_data, "profile", "main", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_opt_set profile").what() << std::endl;
	}
	err = av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_opt_set preset").what() << std::endl;
	}
	// disable b-pyramid. CLI options for this is "-b-pyramid 0"
	//Because Quicktime (ie. iOS) doesn't support this option
	err = av_opt_set(codec_ctx->priv_data, "b-pyramid", "0", 0);
	if (err < 0) {
		//std::cerr << "Error : " << AVException(err, "av_opt_set b-pyramid").what() << std::endl;
	}
	//It's necessary to open stream codec to link it to "codec" (the encoder).
	err = avcodec_open2(codec_ctx, codec, NULL);
	if (err < 0) {
		//throw AVException(err, "avcodec_open2");
	}

	//* dump av format informations
	av_dump_format(fmt_ctx, 0, target, 1);
	//*/
	err = avio_open(&fmt_ctx->pb, target, AVIO_FLAG_WRITE);
	if (err < 0){
		//throw AVException(err, "avio_open");
	}

	//Write file header if necessary
	err = avformat_write_header(fmt_ctx, &fmt_opts);
	if (err < 0){
		//throw AVException(err, "avformat_write_header");
	}

	/* Alloc tmp frame once and for all*/
	tmp_frame = av_frame_alloc();
	if (!tmp_frame){
		//throw AVException(ENOMEM, "alloc frame");
	}
	//Make sure the encoder doesn't keep ref to this frame as we'll modify it.
	av_frame_make_writable(tmp_frame);
	tmp_frame->format = codec_ctx->pix_fmt;
	tmp_frame->width = codec_ctx->width;
	tmp_frame->height = codec_ctx->height;
	err = av_image_alloc(tmp_frame->data, tmp_frame->linesize, codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, 32);
	if (err < 0) {
		//throw AVException(ENOMEM, "can't alloc output frame buffer");
	}
}

Encoder::~Encoder(){
	int err;
	std::cout << "cleaning Encoder" << std::endl;
	//Write pending packets
	while ((err = write((AVFrame*)NULL)) == 1){};
	if (err < 0){
		std::cout << "error writing delayed frame" << std::endl;
	}
	//Write file trailer before exit
	av_write_trailer(this->fmt_ctx);
	//close file
	avio_close(fmt_ctx->pb);
	avformat_free_context(this->fmt_ctx);
	//avcodec_free_context(&this->codec_ctx);

	av_freep(&this->tmp_frame->data[0]);
	av_frame_free(&this->tmp_frame);
}

//Return 1 if a packet was written. 0 if nothing was done.
// return error_code < 0 if there was an error.
int Encoder::write(AVFrame *frame){
	int err;
	int got_output = 1;
	AVPacket pkt = { 0 };
	av_init_packet(&pkt);

	//Set frame pts, monotonically increasing, starting from 0
	//we use frame == NULL to write delayed packets in destructor
	if (frame != NULL) {
		frame->pts = pts++;
	}
	err = avcodec_encode_video2(this->codec_ctx, &pkt, frame, &got_output);
	if (err < 0) {
		//std::cout << AVException(err, "encode frame").what() << std::endl;
		return err;
	}
	if (got_output){
		av_packet_rescale_ts(&pkt, this->codec_ctx->time_base, this->st->time_base);
		pkt.stream_index = this->st->index;
		/* write the frame */
		//printf("Write packet %03d of size : %05d\n",pkt.pts,pkt.size);
		//write_frame will take care of freeing the packet.
		err = av_interleaved_write_frame(this->fmt_ctx, &pkt);
		if (err < 0){
			//std::cout << AVException(err, "write frame").what() << std::endl;
			return err;
		}
		return 1;
	}
	else{
		return 0;
	}
}



void  Encoder::video_encode_example(const char *filename, AVCodecID codec_id)
{
	AVCodec *codec;
	AVCodecContext *c = NULL;
	int i, ret, x, y, got_output;
	FILE *f;
	AVFrame *frame;
	AVPacket pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	printf("Encode video file %s\n", filename);

	/* find the mpeg1 video encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	/* put sample parameters */
	c->bit_rate = 400000;
	/* resolution must be a multiple of two */
	c->width = 352;
	c->height = 288;
	/* frames per second */
	c->time_base = AVRational{ 1, 25 };
	/* emit one intra frame every ten frames
	* check frame pict_type before passing frame
	* to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	* then gop_size is ignored and the output of encoder
	* will always be I frame irrespective to gop_size
	*/
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (codec_id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;

	/* the image can be allocated by any means and av_image_alloc() is
	* just the most convenient way if av_malloc() is to be used */
	ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 32);

	if (ret < 0) {
		fprintf(stderr, "Could not allocate raw picture buffer\n");
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	/* encode 1 second of video */
	for (i = 0; i < 25; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;

		fflush(stdout);
		/* prepare a dummy image */
		/* Y */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
			}
		}

		/* Cb and Cr */
		for (y = 0; y < c->height / 2; y++) {
			for (x = 0; x < c->width / 2; x++) {
				frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
				frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
			}
		}

		frame->pts = i;

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}

	/* add sequence end code to have a real mpeg file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_close(c);
	av_free(c);
	av_freep(&frame->data[0]);
	av_frame_free(&frame);
	printf("\n");
}