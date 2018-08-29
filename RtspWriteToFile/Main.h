#define API_RTSP __declspec(dllexport) 
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif
	// тип функции для обратного вызова при получении картинки
	typedef void(CALLBACK *FGetImageCallback)(unsigned char* buffer, int bufferSize);

	API_RTSP PVOID WINAPI Record(PCHAR input_url, PCHAR output_file);

	API_RTSP PVOID WINAPI Online(PCHAR input_url, FGetImageCallback f_get_image_callback);

#ifdef __cplusplus
}
#endif