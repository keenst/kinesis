#include <stdbool.h>
#include <stdio.h>
#include "glad/gl.h"
#include "glad/wgl.h"
#include "main.h"

bool RUNNING = true;
bool GL_LOADED = false;

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	LRESULT result = 0;

	switch (message) {
		case WM_DESTROY: {
			RUNNING = false;
		} break;
		case WM_CLOSE: {
			RUNNING = false;
		} break;
		case WM_SIZE: {
			if (!GL_LOADED) {
				break;
			}

			glViewport(0, 0, LOWORD(l_param), HIWORD(l_param));
		} break;
		default: {
			result = DefWindowProc(window, message, w_param, l_param);
		} break;
	}

	return result;
}

HWND create_window(HINSTANCE instance) {
	WNDCLASS window_class = {};
	window_class.style = CS_OWNDC;
	window_class.lpfnWndProc = window_proc;
	window_class.hInstance = instance;
	window_class.lpszClassName = "kinesis";

	if (RegisterClass(&window_class)) {
		HWND hwnd = CreateWindowEx(
			0,
			window_class.lpszClassName,
			"Kinesis",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			800,
			600,
			0,
			0,
			instance,
			0);

		return hwnd;
	}

	return 0;
}

void error_message_box(const char* const text) {
	MessageBox(NULL, text, "Error", MB_ICONERROR);
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
	// Open console
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONIN$", "r", stdin);

	// Create window
	HWND window = create_window(instance);
	if (!window) {
		error_message_box("Failed to create window");
		return -1;
	}

	// Get device context
	HDC device_context = GetDC(window);

	// Set pixel format
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;
	int pixel_format = ChoosePixelFormat(device_context, &pfd);

	if (!SetPixelFormat(device_context, pixel_format, &pfd)) {
		error_message_box("Failed to set pixel format");
		return -1;
	}

	// Create a temporary OpenGL context (necessary for loading OpenGL functions)
	HGLRC temp_gl_context = wglCreateContext(device_context);
	if (!temp_gl_context) {
		error_message_box("Failed to create temporary OpenGL context");
		return -1;
	}
	wglMakeCurrent(device_context, temp_gl_context);

	// Load WGL
	gladLoaderLoadWGL(device_context);

	// Set OpenGL attributes
	int attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_FLAGS_ARB,
		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		0,
	};

	// Create final OpenGL context
	HGLRC gl_context = wglCreateContextAttribsARB(device_context, NULL, attributes);
	if (!gl_context) {
		error_message_box("Failed to create final OpenGL context");
		return -1;
	}
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(temp_gl_context);
	wglMakeCurrent(device_context, gl_context);

	// Load glad
	if (!gladLoaderLoadGL()) {
		error_message_box("Failed to load glad");
		return -1;
	}

	GL_LOADED = true;

	startup();

	MSG message;
	while (RUNNING) {
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&message);
		}

		main_loop();

		SwapBuffers(device_context);
	}

	return 0;
}
