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

#ifndef RCORE_DESKTOP_WINDOWS_IMPL_H
#define RCORE_DESKTOP_WINDOWS_IMPL_H

#include <stdio.h>

typedef struct WindowsState {
    int shouldClose;
    int mouseX;
    int mouseY;
    char mouseButtons[8];
} WindowsState;

int Windows_Initialize();
void Windows_Close();
int Windows_CreateWindow(const char* title, int width, int height);
void Windows_SetWindowPos(int x, int y);
void Windows_GetWindowPos(int *x, int *y);
void Windows_SetWindowSize(int width, int height);
void Windows_GetWindowSize(int *width, int *height);
void Windows_SetWindowTitle(const char *title);
void Windows_GetWorkingArea(int *x, int *y, int *width, int *height);
void *Windows_GetWindowHandle();
long long Windows_GetTime();
void Windows_PollEvents();
WindowsState *Windows_CurrentState();
char *Windows_ToMultiByte(wchar_t* data); // Returned value must be freed!
void Windows_SetMousePos(int x, int y);

#endif // RCORE_DESKTOP_WINDOWS_IMPL_H
