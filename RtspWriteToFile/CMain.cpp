#include "Main.h"
#include "CRtspRecorder.h"
#include "CRtspOnline.h"

PVOID __stdcall Record(PCHAR input_url, PCHAR output_file)
{
	CRtspRecorder * recorder = new CRtspRecorder(input_url, output_file);
	if (recorder->Start()){
		return recorder;
	}
	delete recorder;
	return nullptr;
}

PVOID __stdcall Online(PCHAR input_url, FGetImageCallback f_get_image_callback)
{
	CRtspOnline * online = new CRtspOnline(input_url, f_get_image_callback);
	if (online->Start()){
		return online;
	}
	delete online;
	return nullptr;
}