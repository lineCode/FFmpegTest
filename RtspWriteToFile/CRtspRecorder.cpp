#include "CRtspRecorder.h"

CRtspRecorder::CRtspRecorder(PCHAR input_url, PCHAR output_file)
	: CBaseRtsp(input_url)
{
	_output_file = output_file;
}

CRtspRecorder::~CRtspRecorder()
{
}

BOOL CRtspRecorder::Start()
{
	bool verbose = false;

	VSInput* input = open_input("rtsp", _input_url, verbose);
	VSOutput* output = open_output("mp4", _output_file, input, verbose);

	while (true) {

		AVPacket* packet = av_packet_alloc();

		av_init_packet(packet);

		auto readRes = read_packet(input, packet, verbose);

		if (readRes == -1) {
			continue;
		}

		if (readRes == 0) {
			continue;
		}

		if (write_packet(input, output, packet, verbose) > 0) {
			//all ok;
		};

		av_packet_free(&packet);
	}

	destroy_input(input);
	destroy_output(output);
}

// We change the packet's pts, dts, duration, pos.
//
// We do not unref it.
//
// Returns:
// -1 if error
// 1 if we wrote the packet
int CRtspRecorder::write_packet(const struct VSInput * const input, struct VSOutput * const output, AVPacket * const pkt, const bool verbose)
{
	if (!input || !output || !pkt) {
		printf("%s\n", strerror(EINVAL));
		return -1;
	}

	AVStream * const in_stream = input->format_ctx->streams[pkt->stream_index];
	if (!in_stream) {
		printf("input stream not found with stream index %d\n", pkt->stream_index);
		return -1;
	}

	// If there are multiple input streams, then the stream index on the packet
	// may not match the stream index in our output. We need to ensure the index
	// matches. Note by this point we have checked that it is indeed a packet
	// from the stream we want (we do this when reading the packet).
	//
	// As we only ever have a single output stream (one, video), the index will
	// be 0.
	if (pkt->stream_index != 0) {
		if (verbose) {
			printf("updating packet stream index to 0 (from %d)\n", pkt->stream_index);
		}

		pkt->stream_index = 0;
	}

	AVStream * const out_stream = output->format_ctx->streams[pkt->stream_index];
	if (!out_stream) {
		printf("output stream not found with stream index %d\n", pkt->stream_index);
		return -1;
	}

	// It is possible that the input is not well formed. Its dts (decompression
	// timestamp) may fluctuate. av_write_frame() says that the dts must be
	// strictly increasing.
	//
	// Packets from such inputs might look like:
	//
	// in: pts:18750 pts_time:0.208333 dts:18750 dts_time:0.208333 duration:3750 duration_time:0.0416667 stream_index:1
	// in: pts:0 pts_time:0 dts:0 dts_time:0 duration:3750 duration_time:0.0416667 stream_index:1
	//
	// dts here is 18750 and then 0.
	//
	// If we try to write the second packet as is, we'll see this error:
	// [mp4 @ 0x10f1ae0] Application provided invalid, non monotonically increasing dts to muxer in stream 1: 18750 >= 0
	//
	// This is apparently a fairly common problem. In ffmpeg.c (as of ffmpeg
	// 3.2.4 at least) there is logic to rewrite the dts and warn if it happens.
	// Let's do the same. Note my logic is a little different here.
	bool fix_dts = pkt->dts != AV_NOPTS_VALUE && output->last_dts != AV_NOPTS_VALUE && pkt->dts <= output->last_dts;

	// It is also possible for input streams to include a packet with
	// dts/pts=NOPTS after packets with dts/pts set. These won't be caught by the
	// prior case. If we try to send these to the encoder however, we'll generate
	// the same error (non monotonically increasing DTS) since the output packet
	// will have dts/pts=0.
	fix_dts |= pkt->dts == AV_NOPTS_VALUE && output->last_dts != AV_NOPTS_VALUE;

	if (fix_dts) {
		int64_t const next_dts = output->last_dts + 1;

		if (verbose) {
			printf("Warning: Non-monotonous DTS in input stream. Previous: %" PRId64 " current: %" PRId64 ". changing to %" PRId64 ".\n",
				output->last_dts, pkt->dts, next_dts);
		}

		// We also apparently (ffmpeg.c does this too) need to update the pts.
		// Otherwise we see an error like:
		//
		// [mp4 @ 0x555e6825ea60] pts (3780) < dts (22531) in stream 0
		if (pkt->pts != AV_NOPTS_VALUE && pkt->pts >= pkt->dts) {
			pkt->pts = FFMAX(pkt->pts, next_dts);
		}
		// In the case where pkt->dts was AV_NOPTS_VALUE, pkt->pts can be
		// AV_NOPTS_VALUE too which we fix as well.
		if (pkt->pts == AV_NOPTS_VALUE) {
			pkt->pts = next_dts;
		}

		pkt->dts = next_dts;
	}

	if (pkt->pts == AV_NOPTS_VALUE) {
		pkt->pts = 0;
	}
	else {
		pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_PASS_MINMAX);
	}

	if (pkt->dts == AV_NOPTS_VALUE) {
		pkt->dts = 0;
	}
	else {
		pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_PASS_MINMAX);
	}

	pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
	pkt->pos = -1;

	// Track last dts we see (see where we use it for why).
	output->last_dts = pkt->dts;

	// Write encoded frame (as a packet).
	// av_interleaved_write_frame() works too, but I don't think it is needed.
	// Using av_write_frame() skips buffering.
	const int write_res = av_write_frame(output->format_ctx, pkt);
	if (write_res != 0) {
		//printf("unable to write frame: %s\n", av_err2str(write_res));
		return -1;
	}

	return 1;
}
