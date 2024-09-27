#include <stdbool.h>
#include <stdio.h>
#include "glad/gl.h"
#include "glad/wgl.h"

bool Running = true;
bool GLLoaded = false;
HDC DeviceContext;

void display() {
	glClearColor(0.2f, 0.3f, 0.3f, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	SwapBuffers(DeviceContext);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	LRESULT result = 0;
	static PAINTSTRUCT paint;

	switch (message) {
		case WM_DESTROY: {
			Running = false;
		} break;
		case WM_CLOSE: {
			Running = false;
		} break;
		case WM_PAINT: {
			display();
			BeginPaint(window, &paint);
			EndPaint(window, &paint);
		} break;
		case WM_SIZE: {
			if (!GLLoaded) {
				break;
			}

			glViewport(0, 0, LOWORD(l_param), HIWORD(l_param));
			PostMessage(window, WM_PAINT, 0, 0);
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

void* get_any_gl_function_address(const char* name) {
	void* p = (void*)wglGetProcAddress(name);
	if (p == 0 ||
		(p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) ||
		(p == (void*)-1)) {
		HMODULE module = LoadLibraryA("opengl32.dll");
		p = (void*)GetProcAddress(module, name);
	}

	return p;
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
	DeviceContext = GetDC(window);

	// Set pixel format
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;
	int pixel_format = ChoosePixelFormat(DeviceContext, &pfd);

	if (!SetPixelFormat(DeviceContext, pixel_format, &pfd)) {
		error_message_box("Failed to set pixel format");
		return -1;
	}

	// Create a temporary OpenGL context (necessary for loading OpenGL functions)
	HGLRC temp_gl_context = wglCreateContext(DeviceContext);
	if (!temp_gl_context) {
		error_message_box("Failed to create temporary OpenGL context");
		return -1;
	}
	wglMakeCurrent(DeviceContext, temp_gl_context);

	// Load WGL
	gladLoaderLoadWGL(DeviceContext);

	// Set OpenGL attributes
	int attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_FLAGS_ARB,
		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		0,
	};

	// Create final OpenGL context
	HGLRC gl_context = wglCreateContextAttribsARB(DeviceContext, NULL, attributes);
	if (!gl_context) {
		error_message_box("Failed to create final OpenGL context");
		return -1;
	}
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(temp_gl_context);
	wglMakeCurrent(DeviceContext, gl_context);

	// Load glad
	if (!gladLoaderLoadGL()) {
		error_message_box("Failed to load glad");
		return -1;
	}

	GLLoaded = true;

	MSG message;
	while (Running) {
		bool message_result = GetMessage(&message, 0, 0, 0);
		if (message_result) {
			DispatchMessage(&message);
		}
	}

	return 0;
}
