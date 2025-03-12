#include <stdbool.h>
#include <stdio.h>
#include "glad/gl.h"
#include "glad/wgl.h"
#include "main.h"
#include "platform.h"

bool RUNNING = true;
bool GL_LOADED = false;
Inputs INPUT_BUFFER = {};

bool MOUSE_LEFT_DOWN = false;

void get_real_window_size(int width, int height, int* real_width, int* real_height) {
	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	*real_width = rect.right - rect.left;
	*real_height = rect.bottom - rect.top;
}

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
			int width, height;
			get_real_window_size(LOWORD(l_param), HIWORD(l_param), &width, &height);
			glViewport(0, 0, LOWORD(l_param), HIWORD(l_param));
			update_window_size(width, height);
		} break;
		case WM_KEYDOWN: {
			switch (w_param) {
				case VK_SPACE: {
					INPUT_BUFFER.pause = true;
				} break;
				case VK_TAB: {
					INPUT_BUFFER.toggle_wireframe = true;
				} break;
				case 'R': {
					INPUT_BUFFER.reset_simulation = true;
				} break;
				case '1': {
					INPUT_BUFFER.realtime = true;
				} break;
				case '2': {
					INPUT_BUFFER.slowmo_2x = true;
				} break;
				case '3': {
					INPUT_BUFFER.slowmo_3x = true;
				} break;
				case '4': {
					INPUT_BUFFER.slowmo_4x = true;
				} break;
				case '5': {
					INPUT_BUFFER.slowmo_5x = true;
				} break;
			}
		} break;
		case WM_LBUTTONDOWN: {
			MOUSE_LEFT_DOWN = true;
		} break;
		case WM_LBUTTONUP: {
			MOUSE_LEFT_DOWN = false;
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

	int adjusted_width, adjusted_height;
	get_real_window_size(800, 600, &adjusted_width, &adjusted_height);

	if (RegisterClass(&window_class)) {
		HWND hwnd = CreateWindowEx(
			0,
			window_class.lpszClassName,
			"Kinesis",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			adjusted_width,
			adjusted_height,
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

	// Get command line arguments
	const LPWSTR arguments_wide = GetCommandLineW();
	char arguments_narrow[1024];
	WideCharToMultiByte(CP_UTF8, 0, arguments_wide, -1, arguments_narrow, sizeof(arguments_narrow), NULL, NULL);

	int argc;
	LPWSTR* argv = CommandLineToArgvW(arguments_wide, &argc);

	LocalFree(argv);

	startup(argc, argv);

	Inputs old_inputs = {};

	MSG message;
	while (RUNNING) {
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&message);
		}

		POINT cursor_pos;
		GetCursorPos(&cursor_pos);
		INPUT_BUFFER.mouse_pos.x = cursor_pos.x;
		INPUT_BUFFER.mouse_pos.y = cursor_pos.y;

		INPUT_BUFFER.mouse_left = MOUSE_LEFT_DOWN;

		main_loop(old_inputs, INPUT_BUFFER);
		old_inputs = INPUT_BUFFER;
		INPUT_BUFFER = (Inputs){};

		SwapBuffers(device_context);
	}

	return 0;
}
