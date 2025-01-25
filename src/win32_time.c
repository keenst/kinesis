#include <windows.h>

double get_time_ms() {
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);

	return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
}

// TODO: Doesn't have to be in platform specific header
void sleep_ms(const double time_ms) {
	const double target_time = get_time_ms() + time_ms;
	while (get_time_ms() < target_time) {}
}
