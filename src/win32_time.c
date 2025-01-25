#include <windows.h>

double get_time_ms() {
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);

	return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
}

void sleep_ms(double time_ms) {
	Sleep(time_ms);
}
