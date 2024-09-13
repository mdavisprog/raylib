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

typedef enum {
    WINDOWSKEY_NULL = 0,
    WINDOWSKEY_APOSTROPHE,
    WINDOWSKEY_COMMA,
    WINDOWSKEY_MINUS,
    WINDOWSKEY_PERIOD,
    WINDOWSKEY_SLASH,
    WINDOWSKEY_ZERO,
    WINDOWSKEY_ONE,
    WINDOWSKEY_TWO,
    WINDOWSKEY_THREE,
    WINDOWSKEY_FOUR,
    WINDOWSKEY_FIVE,
    WINDOWSKEY_SIX,
    WINDOWSKEY_SEVEN,
    WINDOWSKEY_EIGHT,
    WINDOWSKEY_NINE,
    WINDOWSKEY_SEMICOLON,
    WINDOWSKEY_EQUAL,
    WINDOWSKEY_A,
    WINDOWSKEY_B,
    WINDOWSKEY_C,
    WINDOWSKEY_D,
    WINDOWSKEY_E,
    WINDOWSKEY_F,
    WINDOWSKEY_G,
    WINDOWSKEY_H,
    WINDOWSKEY_I,
    WINDOWSKEY_J,
    WINDOWSKEY_K,
    WINDOWSKEY_L,
    WINDOWSKEY_M,
    WINDOWSKEY_N,
    WINDOWSKEY_O,
    WINDOWSKEY_P,
    WINDOWSKEY_Q,
    WINDOWSKEY_R,
    WINDOWSKEY_S,
    WINDOWSKEY_T,
    WINDOWSKEY_U,
    WINDOWSKEY_V,
    WINDOWSKEY_W,
    WINDOWSKEY_X,
    WINDOWSKEY_Y,
    WINDOWSKEY_Z,
    WINDOWSKEY_LEFT_BRACKET,
    WINDOWSKEY_BACKSLASH,
    WINDOWSKEY_RIGHT_BRACKET,
    WINDOWSKEY_GRAVE,
    WINDOWSKEY_SPACE,
    WINDOWSKEY_ESCAPE,
    WINDOWSKEY_ENTER,
    WINDOWSKEY_TAB,
    WINDOWSKEY_BACKSPACE,
    WINDOWSKEY_INSERT,
    WINDOWSKEY_DELETE,
    WINDOWSKEY_RIGHT,
    WINDOWSKEY_LEFT,
    WINDOWSKEY_DOWN,
    WINDOWSKEY_UP,
    WINDOWSKEY_PAGE_UP,
    WINDOWSKEY_PAGE_DOWN,
    WINDOWSKEY_HOME,
    WINDOWSKEY_END,
    WINDOWSKEY_CAPS_LOCK,
    WINDOWSKEY_SCROLL_LOCK,
    WINDOWSKEY_NUM_LOCK,
    WINDOWSKEY_PRINT_SCREEN,
    WINDOWSKEY_PAUSE,
    WINDOWSKEY_F1,
    WINDOWSKEY_F2,
    WINDOWSKEY_F3,
    WINDOWSKEY_F4,
    WINDOWSKEY_F5,
    WINDOWSKEY_F6,
    WINDOWSKEY_F7,
    WINDOWSKEY_F8,
    WINDOWSKEY_F9,
    WINDOWSKEY_F10,
    WINDOWSKEY_F11,
    WINDOWSKEY_F12,
    WINDOWSKEY_LEFT_SHIFT,
    WINDOWSKEY_LEFT_CONTROL,
    WINDOWSKEY_LEFT_ALT,
    WINDOWSKEY_LEFT_SUPER,
    WINDOWSKEY_RIGHT_SHIFT,
    WINDOWSKEY_RIGHT_CONTROL,
    WINDOWSKEY_RIGHT_ALT,
    WINDOWSKEY_RIGHT_SUPER,
    WINDOWSKEY_KB_MENU,
    WINDOWSKEY_KP_0,
    WINDOWSKEY_KP_1,
    WINDOWSKEY_KP_2,
    WINDOWSKEY_KP_3,
    WINDOWSKEY_KP_4,
    WINDOWSKEY_KP_5,
    WINDOWSKEY_KP_6,
    WINDOWSKEY_KP_7,
    WINDOWSKEY_KP_8,
    WINDOWSKEY_KP_9,
    WINDOWSKEY_KP_DECIMAL,
    WINDOWSKEY_KP_DIVIDE,
    WINDOWSKEY_KP_MULTIPLY,
    WINDOWSKEY_KP_SUBTRACT,
    WINDOWSKEY_KP_ADD,
    WINDOWSKEY_KP_ENTER,
    WINDOWSKEY_KP_EQUAL,
    WINDOWSKEY_MAX
} WindowsKey;

typedef struct WindowsState {
    int shouldClose;
    int mouseX;
    int mouseY;
    int mouseWheel;
    char mouseButtons[8];
    char keys[WINDOWSKEY_MAX];
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
