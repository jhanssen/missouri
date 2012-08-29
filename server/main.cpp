#include "Windows.h"
#include "GDICapturer.h"

int main(int argc, char** argv)
{
	GDICapturer cap;
	const uint8_t* buffer = cap.GetBuffer();
	const int32_t width = cap.GetWidth();
	const int32_t height = cap.GetHeight();
	const int32_t size = cap.GetImageSize();
	const int sleepFor = 1000 / 60;
	for (;;) {
		cap.Capture();
		Sleep(sleepFor);
	}
}
