#include "CBaseRtsp.h"

CBaseRtsp::CBaseRtsp(PCHAR input_url)
{
	_input_url = input_url;
}

CBaseRtsp::~CBaseRtsp()
{
}

void CBaseRtsp::setup(void)
{
	// Set up library.

	// Register muxers, demuxers, and protocols.
	//av_register_all();

	// Make formats available.
	//avdevice_register_all();

	avformat_network_init();
}

struct VSInput* CBaseRtsp::open_input(const char * const input_format_name, const char * const input_url, const bool verbose)
{
	if (!input_format_name || strlen(input_format_name) == 0 || !input_url || strlen(input_url) == 0) {
		printf("%s\n", strerror(EINVAL));
		return NULL;
	}

	struct VSInput * const input = static_cast<VSInput*>(calloc(1, sizeof(struct VSInput)));
	if (!input) {
		printf("%s\n", strerror(errno));
		return NULL;
	}

	AVInputFormat * const input_format = av_find_input_format(input_format_name);
	if (!input_format) {
		destroy_input(input);
		return NULL;
	}


	int const open_status = avformat_open_input(&input->format_ctx, input_url, input_format, NULL);
	if (open_status != 0) {
		printf("unable to open input: %s\n", get_ffmpeg_message(open_status));
		destroy_input(input);
		return NULL;
	}

	if (avformat_find_stream_info(input->format_ctx, NULL) < 0) {
		printf("failed to find stream info\n");
		destroy_input(input);
		return NULL;
	}

	if (verbose) {
		av_dump_format(input->format_ctx, 0, input_url, 0);
	}

	input->video_stream_index = -1;

	for (unsigned int i = 0; i < input->format_ctx->nb_streams; i++) {
		AVStream * const in_stream = input->format_ctx->streams[i];

		if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
			if (verbose) {
				printf("skip non-video stream %u\n", i);
			}
			continue;
		}

		input->video_stream_index = (int)i;
		break;
	}

	if (input->video_stream_index == -1) {
		printf("no video stream found\n");
		destroy_input(input);
		return NULL;
	}
	return input;
}

struct VSOutput* CBaseRtsp::open_output(const char * const output_format_name, const char * const output_url, const struct VSInput * const input, const bool verbose)
{
	if (!output_format_name || strlen(output_format_name) == 0 ||
		!output_url || strlen(output_url) == 0
		|| !input) {
		printf("%s\n", strerror(EINVAL));
		return NULL;
	}

	struct VSOutput * const output = static_cast<VSOutput*>(calloc(1, sizeof(struct VSOutput)));
	if (!output) {
		printf("%s\n", strerror(errno));
		return NULL;
	}

	AVOutputFormat * const output_format = av_guess_format(output_format_name, NULL, NULL);
	if (!output_format) {
		printf("output format not found\n");
		destroy_output(output);
		return NULL;
	}

	//if (output_format_name == "rtp") {
		if (avformat_alloc_output_context2(&output->format_ctx, output_format, output_format->name, output_url) < 0) {
			printf("unable to create output context\n");
			destroy_output(output);
			return NULL;
		}
	/*}
	else {
		if (avformat_alloc_output_context2(&output->format_ctx, output_format, NULL, NULL) < 0) {
			printf("unable to create output context\n");
			destroy_output(output);
			return NULL;
		}
	}*/

	auto codec = avcodec_find_encoder(OUTPUT_CODEC);

	AVStream * const out_stream = avformat_new_stream(output->format_ctx, codec);
	if (!out_stream) {
		printf("unable to add stream\n");
		destroy_output(output);
		return NULL;
	}

	AVStream * const in_stream = input->format_ctx->streams[input->video_stream_index];

	if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
		printf("unable to copy codec parameters\n");
		destroy_output(output);
		return NULL;
	}

	if (verbose) {
		av_dump_format(output->format_ctx, 0, output_url, 1);
	}

	// Open output file.
	if (avio_open(&output->format_ctx->pb, output_url, AVIO_FLAG_WRITE) < 0) {
		printf("unable to open output file\n");
		destroy_output(output);
		return NULL;
	}

	// Write file header.
	AVDictionary * opts = NULL;

	// -movflags frag_keyframe tells the mp4 muxer to fragment at each video
	// keyframe. This is necessary for it to support output to a non-seekable
	// file (e.g., pipe).
	//
	// -movflags isml+frag_keyframe is the same, except isml appears to be to
	// make the output a live smooth streaming feed (as opposed to not live). I'm
	// not sure the difference, but isml appears to be a microsoft
	// format/protocol.
	//
	// To specify both, use isml+frag_keyframe as the value.
	//
	// I found that while Chrome had no trouble displaying the resulting mp4 with
	// just frag_keyframe, Firefox would not until I also added empty_moov.
	// empty_moov apparently writes some info at the start of the file.
	if (av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0) < 0) {
		printf("unable to set movflags opt\n");
		destroy_output(output);
		return NULL;
	}

	if (av_dict_set_int(&opts, "flush_packets", 1, 0) < 0) {
		printf("unable to set flush_packets opt\n");
		destroy_output(output);
		av_dict_free(&opts);
		return NULL;
	}

	//default brand is "isom", which fails on some devices
	if (av_dict_set(&opts, "brand", "mp42", 0) < 0) {
		printf("unable to set brand opt\n");
		destroy_output(output);
		av_dict_free(&opts);
		return NULL;
	}

	if (avformat_write_header(output->format_ctx, &opts) < 0) {
		printf("unable to write header\n");
		destroy_output(output);
		av_dict_free(&opts);
		return NULL;
	}

	//////// Check any options that were not set. Because I'm not sure if all are
	//////// appropriate to set through the avformat_write_header().
	//////auto c = av_dict_count(opts);
	//////if (c != 0) {
	//////	printf("some options not set\n");
	//////	destroy_output(output);
	//////	av_dict_free(&opts);
	//////	return NULL;
	//////}

	av_dict_free(&opts);

	//H.264 specific options
	out_stream->codec->gop_size = 25;
	out_stream->codec->level = 31;

	auto err = av_opt_set(out_stream->codec->priv_data, "crf", "12", 0);
	if (err < 0) {
		auto c = get_ffmpeg_message(err);
		printf(c);
		destroy_output(output);
		return NULL;
	}

	err = av_opt_set(out_stream->codec->priv_data, "profile", "main", 0);
	if (err < 0) {
		auto c = get_ffmpeg_message(err);
		printf(c);
		destroy_output(output);
		return NULL;
	}

	err = av_opt_set(out_stream->codec->priv_data, "preset", "slow", 0);
	if (err < 0) {
		auto c = get_ffmpeg_message(err);
		printf(c);
		destroy_output(output);
		return NULL;
	}

	output->last_dts = AV_NOPTS_VALUE;

	return output;
}

// Read a compressed and encoded frame as a packet.
//
// Returns:
// -1 if error
// 0 if nothing useful read (e.g., non-video packet)
// 1 if read a packet
int CBaseRtsp::read_packet(const struct VSInput * input, AVPacket * const pkt, const bool verbose)
{
	if (!input || !pkt) {
		printf("%s\n", strerror(errno));
		return -1;
	}

	memset(pkt, 0, sizeof(AVPacket));

	// Read encoded frame (as a packet).
	if (av_read_frame(input->format_ctx, pkt) != 0) {
		printf("unable to read frame\n");
		return -1;
	}


	// Ignore it if it's not our video stream.
	if (pkt->stream_index != input->video_stream_index) {
		if (verbose) {
			printf("skipping packet from input stream %d, our video is from stream %d\n", pkt->stream_index, input->video_stream_index);
		}

		av_packet_unref(pkt);
		return 0;
	}

	return 1;
}

void CBaseRtsp::destroy_input(struct VSInput * const input)
{
	if (!input) {
		return;
	}

	if (input->format_ctx) {
		avformat_close_input(&input->format_ctx);
		avformat_free_context(input->format_ctx);
	}

	free(input);
}

void CBaseRtsp::destroy_output(struct VSOutput * const output)
{
	if (!output) {
		return;
	}

	if (output->format_ctx) {
		if (av_write_trailer(output->format_ctx) != 0) {
			printf("unable to write trailer\n");
		}

		if (avio_closep(&output->format_ctx->pb) != 0) {
			printf("avio_closep failed\n");
		}

		avformat_free_context(output->format_ctx);
	}

	free(output);
}

char* CBaseRtsp::get_ffmpeg_message(int error)
{
	if (error != 0){
		char* tmp_mess = new char[256];
		av_strerror(error, tmp_mess, 256);
		return tmp_mess;
	}

	return nullptr;
}