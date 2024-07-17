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

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

typedef struct
{
    HWND handle;
} PlatformData;

static PlatformData platform = { 0 };

static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

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
    wchar_t* wTitle = (wchar_t*)malloc(sizeof(wchar_t) * length);
    MultiByteToWideChar(CP_ACP, 0, title, titleLength, wTitle, length);

    DWORD style = WS_OVERLAPPEDWINDOW;
    platform.handle = CreateWindowExW(0, WND_CLASS_NAME, wTitle, style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
    free(wTitle);

    if (platform.handle == NULL)
    {
        return -1;
    }

    return 0;
}
