/**********************************************************************************************
*
*   rcore_desktop_windows_impl - Functions to implement Windows specific functionality to prevent
*                                name clashing with existing raylib functions.
*
*   PLATFORM: Windows
*
*   CONFIGURATION:
*
*   DEPENDENCIES:
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "rcore_desktop_windows_impl.h"

#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define WND_CLASS_NAME L"raylib"

#include <stdlib.h>
#include <Windows.h>
#include <windowsx.h>
#include <hidusage.h>

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------

typedef struct {
    HWND handle;
    WindowsState state;
    BOOL usingRawInput;
} PlatformData;

//----------------------------------------------------------------------------------
// Variables
//----------------------------------------------------------------------------------

static PlatformData platform = { 0 };

//----------------------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------------------

static char* ToMultiByte(wchar_t* data)
{
    const int dataLength = (int)wcslen(data);
    const int length = WideCharToMultiByte(CP_ACP, 0, data, dataLength, NULL, 0, NULL, NULL);
    char* result = (char*)malloc(sizeof(char) * (length + 1));
    WideCharToMultiByte(CP_ACP, 0, data, dataLength, result, length, NULL, NULL);
    result[length] = 0;
    return result;
}

static wchar_t *ToWide(const char *data)
{
    const int dataLength = (int)strlen(data);
    const int length = MultiByteToWideChar(CP_ACP, 0, data, dataLength, NULL, 0);
    wchar_t *result = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));
    MultiByteToWideChar(CP_ACP, 0, data, dataLength, result, length);
    result[length] = 0;
    return result;
}

static WindowsKey ConvertKey(WPARAM key)
{
    switch (key)
    {
    //WINDOWSKEY_APOSTROPHE;
    //WINDOWSKEY_COMMA,
    //WINDOWSKEY_MINUS,
    //WINDOWSKEY_PERIOD,
    //WINDOWSKEY_SLASH,
    //WINDOWSKEY_SEMICOLON,
    //WINDOWSKEY_EQUAL,
    case 0x30: return WINDOWSKEY_ZERO;
    case 0x31: return WINDOWSKEY_ONE;
    case 0x32: return WINDOWSKEY_TWO;
    case 0x33: return WINDOWSKEY_THREE;
    case 0x34: return WINDOWSKEY_FOUR;
    case 0x35: return WINDOWSKEY_FIVE;
    case 0x36: return WINDOWSKEY_SIX;
    case 0x37: return WINDOWSKEY_SEVEN;
    case 0x38: return WINDOWSKEY_EIGHT;
    case 0x39: return WINDOWSKEY_NINE;
    case 0x41: return WINDOWSKEY_A;
    case 0x42: return WINDOWSKEY_B;
    case 0x43: return WINDOWSKEY_C;
    case 0x44: return WINDOWSKEY_D;
    case 0x45: return WINDOWSKEY_E;
    case 0x46: return WINDOWSKEY_F;
    case 0x47: return WINDOWSKEY_G;
    case 0x48: return WINDOWSKEY_H;
    case 0x49: return WINDOWSKEY_I;
    case 0x4A: return WINDOWSKEY_J;
    case 0x4B: return WINDOWSKEY_K;
    case 0x4C: return WINDOWSKEY_L;
    case 0x4D: return WINDOWSKEY_M;
    case 0x4E: return WINDOWSKEY_N;
    case 0x4F: return WINDOWSKEY_O;
    case 0x50: return WINDOWSKEY_P;
    case 0x51: return WINDOWSKEY_Q;
    case 0x52: return WINDOWSKEY_R;
    case 0x53: return WINDOWSKEY_S;
    case 0x54: return WINDOWSKEY_T;
    case 0x55: return WINDOWSKEY_U;
    case 0x56: return WINDOWSKEY_V;
    case 0x57: return WINDOWSKEY_W;
    case 0x58: return WINDOWSKEY_X;
    case 0x59: return WINDOWSKEY_Y;
    case 0x5A: return WINDOWSKEY_Z;
    //WINDOWSKEY_LEFT_BRACKET,
    //WINDOWSKEY_BACKSLASH,
    //WINDOWSKEY_RIGHT_BRACKET,
    //WINDOWSKEY_GRAVE,
    case VK_SPACE: return WINDOWSKEY_SPACE;
    case VK_ESCAPE: return WINDOWSKEY_ESCAPE;
    case VK_RETURN: return WINDOWSKEY_ENTER;
    case VK_TAB: return WINDOWSKEY_TAB;
    case VK_BACK: return WINDOWSKEY_BACKSPACE;
    case VK_INSERT: return WINDOWSKEY_INSERT;
    case VK_DELETE: return WINDOWSKEY_DELETE;
    case VK_RIGHT: return WINDOWSKEY_RIGHT;
    case VK_LEFT: return WINDOWSKEY_LEFT;
    case VK_DOWN: return WINDOWSKEY_DOWN;
    case VK_UP: return WINDOWSKEY_UP;
    case VK_PRIOR: return WINDOWSKEY_PAGE_UP;
    case VK_NEXT: return WINDOWSKEY_PAGE_DOWN;
    case VK_HOME: return WINDOWSKEY_HOME;
    case VK_END: return WINDOWSKEY_END;
    case VK_CAPITAL: return WINDOWSKEY_CAPS_LOCK;
    case VK_SCROLL: return WINDOWSKEY_SCROLL_LOCK;
    case VK_NUMLOCK: return WINDOWSKEY_NUM_LOCK;
    case VK_SNAPSHOT: return WINDOWSKEY_PRINT_SCREEN;
    //WINDOWSKEY_PAUSE,
    case VK_F1: return WINDOWSKEY_F1;
    case VK_F2: return WINDOWSKEY_F2;
    case VK_F3: return WINDOWSKEY_F3;
    case VK_F4: return WINDOWSKEY_F4;
    case VK_F5: return WINDOWSKEY_F5;
    case VK_F6: return WINDOWSKEY_F6;
    case VK_F7: return WINDOWSKEY_F7;
    case VK_F8: return WINDOWSKEY_F8;
    case VK_F9: return WINDOWSKEY_F9;
    case VK_F10: return WINDOWSKEY_F10;
    case VK_F11: return WINDOWSKEY_F11;
    case VK_F12: return WINDOWSKEY_F12;
    case VK_LSHIFT: return WINDOWSKEY_LEFT_SHIFT;
    case VK_LCONTROL: return WINDOWSKEY_LEFT_CONTROL;
    case VK_LMENU: return WINDOWSKEY_LEFT_ALT;
    case VK_LWIN: return WINDOWSKEY_LEFT_SUPER;
    case VK_RSHIFT: return WINDOWSKEY_RIGHT_SHIFT;
    case VK_RCONTROL: return WINDOWSKEY_RIGHT_CONTROL;
    case VK_RMENU: return WINDOWSKEY_RIGHT_ALT;
    case VK_RWIN: return WINDOWSKEY_RIGHT_SUPER;
    //WINDOWSKEY_KB_MENU,
    case VK_NUMPAD0: return WINDOWSKEY_KP_0;
    case VK_NUMPAD1: return WINDOWSKEY_KP_1;
    case VK_NUMPAD2: return WINDOWSKEY_KP_2;
    case VK_NUMPAD3: return WINDOWSKEY_KP_3;
    case VK_NUMPAD4: return WINDOWSKEY_KP_4;
    case VK_NUMPAD5: return WINDOWSKEY_KP_5;
    case VK_NUMPAD6: return WINDOWSKEY_KP_6;
    case VK_NUMPAD7: return WINDOWSKEY_KP_7;
    case VK_NUMPAD8: return WINDOWSKEY_KP_8;
    case VK_NUMPAD9: return WINDOWSKEY_KP_9;
    case VK_DECIMAL: return WINDOWSKEY_KP_DECIMAL;
    case VK_DIVIDE: return WINDOWSKEY_KP_DIVIDE;
    case VK_MULTIPLY: return WINDOWSKEY_KP_MULTIPLY;
    case VK_SUBTRACT: return WINDOWSKEY_KP_SUBTRACT;
    case VK_ADD: return WINDOWSKEY_KP_ADD;
    //WINDOWSKEY_KP_ENTER,
    //WINDOWSKEY_KP_EQUAL,
    default: break;
    }

    return WINDOWSKEY_NULL;
}

static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
    {
        platform.state.shouldClose = TRUE;
    } break;

    case WM_MOUSEMOVE:
    {
        if (!platform.usingRawInput)
        {
            platform.state.mouseX = GET_X_LPARAM(lParam);
            platform.state.mouseY = GET_Y_LPARAM(lParam);
        }
    } break;

    case WM_INPUT:
    {
        if (platform.usingRawInput)
        {
            RAWINPUT rawInput = { 0 };
            UINT size = sizeof(rawInput);

            UINT result = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &size, sizeof(RAWINPUTHEADER));
            if (result != (UINT)-1)
            {
                if (rawInput.header.dwType == RIM_TYPEMOUSE)
                {
                    if (!(rawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
                    {
                        platform.state.mouseX += rawInput.data.mouse.lLastX;
                        platform.state.mouseY += rawInput.data.mouse.lLastY;
                    }
                }
            }
        }
    } break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    {
        int button = 0;
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)
        {
            button = 1;
        }
        else if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP)
        {
            button = 2;
        }
        else if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP)
        {
            button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4;
        }

        const char pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN;

        platform.state.mouseButtons[button] = pressed;
    } break;

    case WM_MOUSEWHEEL:
    {
        platform.state.mouseWheel = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        WindowsKey key = ConvertKey(wParam);
        platform.state.keys[key] = msg == WM_KEYDOWN;
    } break;

    default: break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static BOOL RegisterRawInputDevice(HWND handle, BOOL remove)
{
    RAWINPUTDEVICE rid = { 0 };
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = remove ? RIDEV_REMOVE : RIDEV_INPUTSINK;
    rid.hwndTarget = handle;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        return FALSE;
    }

    return TRUE;
}

static void CaptureCursor(HWND handle, BOOL capture)
{
    if (capture)
    {
        RECT rect = { 0 };
        GetClientRect(handle, &rect);
        POINT min = { rect.left, rect.top };
        POINT max = { rect.right, rect.bottom };
        ClientToScreen(handle, &min);
        ClientToScreen(handle, &max);
        rect.left = min.x;
        rect.top = min.y;
        rect.right = max.x;
        rect.bottom = max.y;
        ClipCursor(&rect);
    }
    else
    {
        ClipCursor(NULL);
    }
}

static void CenterCursor()
{
    int width, height;
    Windows_GetWindowSize(&width, &height);
    Windows_SetMousePos(width / 2, height / 2);
}

//----------------------------------------------------------------------------------
// API
//----------------------------------------------------------------------------------

int Windows_Initialize()
{
    WNDCLASSEXW class = { 0 };
    class.cbSize = sizeof(class);
    class.style = CS_HREDRAW | CS_VREDRAW;
    class.lpszClassName = WND_CLASS_NAME;
    class.lpfnWndProc = WndProc;
    class.hInstance = GetModuleHandleW(NULL);
    class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    class.hIcon = NULL;
    class.hIconSm = NULL;
    class.hbrBackground = NULL;
    class.lpszMenuName = NULL;

    if (RegisterClassExW(&class) == 0)
    {
        printf("WINDOWS: Failed to register class!\n");
        return -1;
    }

    return 0;
}

void Windows_Close()
{
    DestroyWindow(platform.handle);
    UnregisterClassW(WND_CLASS_NAME, NULL);

    platform.handle = NULL;
}

int Windows_CreateWindow(const char* title, int width, int height)
{
    if (platform.handle != NULL)
    {
        return 0;
    }

    const int titleLength = (int)strlen(title);
    const int length = MultiByteToWideChar(CP_ACP, 0, title, titleLength, NULL, 0);
    wchar_t* wTitle = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));
    MultiByteToWideChar(CP_ACP, 0, title, titleLength, wTitle, length);
    wTitle[length] = 0;

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;

    RECT rect = { 0, 0, width, height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    platform.handle = CreateWindowExW(exStyle, WND_CLASS_NAME, wTitle, style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
    free(wTitle);

    if (platform.handle == NULL)
    {
        printf("WINDOWS: Failed to create window!\n");
        return -1;
    }

    ShowWindow(platform.handle, SW_SHOW);

    return 0;
}

void Windows_SetWindowPos(int x, int y)
{
    SetWindowPos(platform.handle, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
}

void Windows_GetWindowPos(int *x, int *y)
{
    RECT bounds = { 0 };
    GetWindowRect(platform.handle, &bounds);

    if (x != NULL)
    {
        *x = bounds.left;
    }

    if (y != NULL)
    {
        *y = bounds.top;
    }
}

void Windows_SetWindowSize(int width, int height)
{
    SetWindowPos(platform.handle, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
}

void Windows_GetWindowSize(int *width, int *height)
{
    RECT bounds = { 0 };
    GetWindowRect(platform.handle, &bounds);

    if (width != NULL)
    {
        *width = bounds.right - bounds.left;
    }

    if (height != NULL)
    {
        *height = bounds.bottom - bounds.top;
    }
}

void Windows_SetWindowTitle(const char *title)
{
    wchar_t *wTitle = ToWide(title);
    SetWindowTextW(platform.handle, wTitle);
    free(wTitle);
}

void Windows_GetWorkingArea(int *x, int *y, int *width, int *height)
{
    HMONITOR monitor = MonitorFromWindow(platform.handle, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = { 0 };
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    if (x != NULL) *x = info.rcWork.left;
    if (y != NULL) *y = info.rcWork.top;
    if (width != NULL) *width = info.rcWork.right - info.rcWork.left;
    if (height != NULL) *height = info.rcWork.bottom - info.rcWork.top;
}

void *Windows_GetWindowHandle()
{
    return (void*)platform.handle;
}

long long Windows_GetTime()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return counter.QuadPart * (LONGLONG)1e9 / frequency.QuadPart;
}

void Windows_PollEvents()
{
    // Reset values with some input events.
    platform.state.mouseWheel = 0;

    MSG msg = { 0 };
    while (PeekMessageW(&msg, platform.handle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

WindowsState* Windows_CurrentState()
{
    return &platform.state;
}

char* Windows_ToMultiByte(wchar_t* data)
{
    return ToMultiByte(data);
}

void Windows_SetMousePos(int x, int y)
{
    platform.state.mouseX = x;
    platform.state.mouseY = y;
    POINT point = { x, y };
    ClientToScreen(platform.handle, &point);
    SetCursorPos(point.x, point.y);
}

void Windows_EnableRawInput()
{
    RegisterRawInputDevice(platform.handle, FALSE);
    CaptureCursor(platform.handle, TRUE);
    platform.usingRawInput = TRUE;
}

void Windows_DisableRawInput()
{
    RegisterRawInputDevice(platform.handle, TRUE);
    CaptureCursor(platform.handle, FALSE);
    platform.usingRawInput = FALSE;
}
