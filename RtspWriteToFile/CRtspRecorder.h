#include "CBaseRtsp.h"

class CRtspRecorder : public CBaseRtsp
{
protected:

	//path to file
	PCHAR _output_file;

	int write_packet(const struct VSInput * const, struct VSOutput * const, AVPacket * const, const bool);

public:

	CRtspRecorder(PCHAR input_url, PCHAR output_file);
	~CRtspRecorder();

	BOOL Start() override;
};