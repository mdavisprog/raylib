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

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------

typedef struct {
    HWND handle;
    WindowsState state;
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

static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE: {
        platform.state.shouldClose = TRUE;
    } break;

    default: break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
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
    platform.handle = CreateWindowExW(0, WND_CLASS_NAME, wTitle, style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
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
