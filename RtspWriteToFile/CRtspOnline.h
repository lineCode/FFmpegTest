#include "CBaseRtsp.h"


class CRtspOnline : public CBaseRtsp
{
protected:

	FGetImageCallback _f_get_image_callback;

	// We change the packet's pts, dts, duration, pos.
	//
	// We do not unref it.
	//
	// Returns:
	// -1 if error
	// 1 if we wrote the packet
	int write_packet(const struct VSInput * const input, FGetImageCallback f_get_image_callback, AVPacket * const pkt, const bool verbose);

	// We change the packet's pts, dts, duration, pos.
	//
	// We do not unref it.
	//
	// Returns:
	// -1 if error
	// 1 if we wrote the packet
	int write_packet2(const struct VSInput * const input, struct VSOutput * const output, AVPacket * const pkt, const bool verbose);

public:

	CRtspOnline(PCHAR input_url, FGetImageCallback f_get_image_callback);
	~CRtspOnline();

	BOOL Start() override;
};