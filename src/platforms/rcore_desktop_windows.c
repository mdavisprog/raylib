/**********************************************************************************************
*
*   rcore_<platform> template - Functions to manage window, graphics device and inputs
*
*   PLATFORM: <PLATFORM>
*       - TODO: Define the target platform for the core
*
*   LIMITATIONS:
*       - Limitation 01
*       - Limitation 02
*
*   POSSIBLE IMPROVEMENTS:
*       - Improvement 01
*       - Improvement 02
*
*   ADDITIONAL NOTES:
*       - TRACELOG() function is located in raylib [utils] module
*
*   CONFIGURATION:
*       #define RCORE_PLATFORM_CUSTOM_FLAG
*           Custom flag for rcore on target platform -not used-
*
*   DEPENDENCIES:
*       - <platform-specific SDK dependency>
*       - gestures: Gestures system for touch-ready devices (or simulated from mouse inputs)
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
#include "../rldx.h"

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    // TODO: Define the platform specific variables required
    void* dummy;
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = { 0 };   // Platform specific data

//----------------------------------------------------------------------------------
// Utility Functions
//----------------------------------------------------------------------------------

static KeyboardKey ConvertKey(WindowsKey key)
{
    switch (key)
    {
    case WINDOWSKEY_APOSTROPHE: return KEY_APOSTROPHE;
    case WINDOWSKEY_COMMA: return KEY_COMMA;
    case WINDOWSKEY_MINUS: return KEY_MINUS;
    case WINDOWSKEY_PERIOD: return KEY_PERIOD;
    case WINDOWSKEY_SLASH: return KEY_SLASH;
    case WINDOWSKEY_ZERO: return KEY_ZERO;
    case WINDOWSKEY_ONE: return KEY_ONE;
    case WINDOWSKEY_TWO: return KEY_TWO;
    case WINDOWSKEY_THREE: return KEY_THREE;
    case WINDOWSKEY_FOUR: return KEY_FOUR;
    case WINDOWSKEY_FIVE: return KEY_FIVE;
    case WINDOWSKEY_SIX: return KEY_SIX;
    case WINDOWSKEY_SEVEN: return KEY_SEVEN;
    case WINDOWSKEY_EIGHT: return KEY_EIGHT;
    case WINDOWSKEY_NINE: return KEY_NINE;
    case WINDOWSKEY_SEMICOLON: return KEY_SEMICOLON;
    case WINDOWSKEY_EQUAL: return KEY_EQUAL;
    case WINDOWSKEY_A: return KEY_A;
    case WINDOWSKEY_B: return KEY_B;
    case WINDOWSKEY_C: return KEY_C;
    case WINDOWSKEY_D: return KEY_D;
    case WINDOWSKEY_E: return KEY_E;
    case WINDOWSKEY_F: return KEY_F;
    case WINDOWSKEY_G: return KEY_G;
    case WINDOWSKEY_H: return KEY_H;
    case WINDOWSKEY_I: return KEY_I;
    case WINDOWSKEY_J: return KEY_J;
    case WINDOWSKEY_K: return KEY_K;
    case WINDOWSKEY_L: return KEY_L;
    case WINDOWSKEY_M: return KEY_M;
    case WINDOWSKEY_N: return KEY_N;
    case WINDOWSKEY_O: return KEY_O;
    case WINDOWSKEY_P: return KEY_P;
    case WINDOWSKEY_Q: return KEY_Q;
    case WINDOWSKEY_R: return KEY_R;
    case WINDOWSKEY_S: return KEY_S;
    case WINDOWSKEY_T: return KEY_T;
    case WINDOWSKEY_U: return KEY_U;
    case WINDOWSKEY_V: return KEY_V;
    case WINDOWSKEY_W: return KEY_W;
    case WINDOWSKEY_X: return KEY_X;
    case WINDOWSKEY_Y: return KEY_Y;
    case WINDOWSKEY_Z: return KEY_Z;
    case WINDOWSKEY_LEFT_BRACKET: return KEY_LEFT_BRACKET;
    case WINDOWSKEY_BACKSLASH: return KEY_BACKSLASH;
    case WINDOWSKEY_RIGHT_BRACKET: return KEY_RIGHT_BRACKET;
    case WINDOWSKEY_GRAVE: return KEY_GRAVE;
    case WINDOWSKEY_SPACE: return KEY_SPACE;
    case WINDOWSKEY_ESCAPE: return KEY_ESCAPE;
    case WINDOWSKEY_ENTER: return KEY_ENTER;
    case WINDOWSKEY_TAB: return KEY_TAB;
    case WINDOWSKEY_BACKSPACE: return KEY_BACKSPACE;
    case WINDOWSKEY_INSERT: return KEY_INSERT;
    case WINDOWSKEY_DELETE: return KEY_DELETE;
    case WINDOWSKEY_RIGHT: return KEY_RIGHT;
    case WINDOWSKEY_LEFT: return KEY_LEFT;
    case WINDOWSKEY_DOWN: return KEY_DOWN;
    case WINDOWSKEY_UP: return KEY_UP;
    case WINDOWSKEY_PAGE_UP: return KEY_PAGE_UP;
    case WINDOWSKEY_PAGE_DOWN: return KEY_PAGE_DOWN;
    case WINDOWSKEY_HOME: return KEY_HOME;
    case WINDOWSKEY_END: return KEY_END;
    case WINDOWSKEY_CAPS_LOCK: return KEY_CAPS_LOCK;
    case WINDOWSKEY_SCROLL_LOCK: return KEY_SCROLL_LOCK;
    case WINDOWSKEY_NUM_LOCK: return KEY_NUM_LOCK;
    case WINDOWSKEY_PRINT_SCREEN: return KEY_PRINT_SCREEN;
    case WINDOWSKEY_PAUSE: return KEY_PAUSE;
    case WINDOWSKEY_F1: return KEY_F1;
    case WINDOWSKEY_F2: return KEY_F2;
    case WINDOWSKEY_F3: return KEY_F3;
    case WINDOWSKEY_F4: return KEY_F4;
    case WINDOWSKEY_F5: return KEY_F5;
    case WINDOWSKEY_F6: return KEY_F6;
    case WINDOWSKEY_F7: return KEY_F7;
    case WINDOWSKEY_F8: return KEY_F8;
    case WINDOWSKEY_F9: return KEY_F9;
    case WINDOWSKEY_F10: return KEY_F10;
    case WINDOWSKEY_F11: return KEY_F11;
    case WINDOWSKEY_F12: return KEY_F12;
    case WINDOWSKEY_LEFT_SHIFT: return KEY_LEFT_SHIFT;
    case WINDOWSKEY_LEFT_CONTROL: return KEY_LEFT_CONTROL;
    case WINDOWSKEY_LEFT_ALT: return KEY_LEFT_ALT;
    case WINDOWSKEY_LEFT_SUPER: return KEY_LEFT_SUPER;
    case WINDOWSKEY_RIGHT_SHIFT: return KEY_RIGHT_SHIFT;
    case WINDOWSKEY_RIGHT_CONTROL: return KEY_RIGHT_CONTROL;
    case WINDOWSKEY_RIGHT_ALT: return KEY_RIGHT_ALT;
    case WINDOWSKEY_RIGHT_SUPER: return KEY_RIGHT_SUPER;
    case WINDOWSKEY_KB_MENU: return KEY_KB_MENU;
    case WINDOWSKEY_KP_0: return KEY_KP_0;
    case WINDOWSKEY_KP_1: return KEY_KP_1;
    case WINDOWSKEY_KP_2: return KEY_KP_2;
    case WINDOWSKEY_KP_3: return KEY_KP_3;
    case WINDOWSKEY_KP_4: return KEY_KP_4;
    case WINDOWSKEY_KP_5: return KEY_KP_5;
    case WINDOWSKEY_KP_6: return KEY_KP_6;
    case WINDOWSKEY_KP_7: return KEY_KP_7;
    case WINDOWSKEY_KP_8: return KEY_KP_8;
    case WINDOWSKEY_KP_9: return KEY_KP_9;
    case WINDOWSKEY_KP_DECIMAL: return KEY_KP_DECIMAL;
    case WINDOWSKEY_KP_DIVIDE: return KEY_KP_DIVIDE;
    case WINDOWSKEY_KP_MULTIPLY: return KEY_KP_MULTIPLY;
    case WINDOWSKEY_KP_SUBTRACT: return KEY_KP_SUBTRACT;
    case WINDOWSKEY_KP_ADD: return KEY_KP_ADD;
    case WINDOWSKEY_KP_ENTER: return KEY_KP_ENTER;
    case WINDOWSKEY_KP_EQUAL: return KEY_KP_EQUAL;
    case WINDOWSKEY_NULL:
    default: break;
    }

    return KEY_NULL;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------

int InitPlatform(void);          // Initialize platform (graphics, inputs and more)
bool InitGraphicsDevice(void);   // Initialize graphics device

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform");
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform");
}

// Set window state: minimized
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform");
}

// Set window state: not minimized/maximized
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform");
}

// Set icon for window
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on target platform");
}

// Set icon for window
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on target platform");
}

// Set title for window
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
    Windows_SetWindowTitle(title);
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    Windows_SetWindowPos(x, y);
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on target platform");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    Windows_SetWindowSize(width, height);
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on target platform");
}

// Set window focused
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on target platform");
}

// Get native window handle
void *GetWindowHandle(void)
{
    return Windows_GetWindowHandle();
}

// Get number of monitors
int GetMonitorCount(void)
{
    TRACELOG(LOG_WARNING, "GetMonitorCount() not implemented on target platform");
    return 1;
}

// Get number of monitors
int GetCurrentMonitor(void)
{
    TRACELOG(LOG_WARNING, "GetCurrentMonitor() not implemented on target platform");
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPosition() not implemented on target platform");
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate() not implemented on target platform");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char *GetMonitorName(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorName() not implemented on target platform");
    return "";
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    int x, y;
    Windows_GetWindowPos(&x, &y);
    return (Vector2){ (float)x, (float)y };
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void)
{
    TRACELOG(LOG_WARNING, "GetWindowScaleDPI() not implemented on target platform");
    return (Vector2){ 1.0f, 1.0f };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    TRACELOG(LOG_WARNING, "SetClipboardText() not implemented on target platform");
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{
    TRACELOG(LOG_WARNING, "GetClipboardText() not implemented on target platform");
    return NULL;
}

// Show mouse cursor
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enables cursor (unlock cursor)
void EnableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);
    Windows_DisableRawInput();

    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (lock cursor)
void DisableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);
    Windows_EnableRawInput();

    CORE.Input.Mouse.cursorHidden = true;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    rlPresent();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void)
{
    long long time = Windows_GetTime();
    return (double)(time - CORE.Time.base) * 1e-9;
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char *url)
{
    // Security check to (partially) avoid malicious code on target platform
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'] character");
    else
    {
        // TODO:
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on target platform");
    return 0;
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
    Windows_SetMousePos(x, y);
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    TRACELOG(LOG_WARNING, "SetMouseCursor() not implemented on target platform");
}

// Register all input events
void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is just called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = (Vector2){ 0.0f, 0.0f };

    // Register previous mouse states
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // TODO: Poll input events for current platform
    Windows_PollEvents();

    WindowsState *state = Windows_CurrentState();
    CORE.Window.shouldClose = state->shouldClose;

    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
    CORE.Input.Mouse.currentPosition.x = (float)state->mouseX;
    CORE.Input.Mouse.currentPosition.y = (float)state->mouseY;

    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
    {
        CORE.Input.Mouse.currentButtonState[i] = state->mouseButtons[i];
    }

    CORE.Input.Mouse.currentWheelMove.y = (float)state->mouseWheel;

    for (int i = 0; i < WINDOWSKEY_MAX; i++)
    {
        KeyboardKey key = ConvertKey(i);

        if (key != KEY_NULL)
        {
            CORE.Input.Keyboard.currentKeyState[key] = state->keys[i];

            if ((CORE.Input.Keyboard.currentKeyState[key] == 0) && (CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE))
            {
                CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = key;
                CORE.Input.Keyboard.keyPressedQueueCount++;
            }

            if (key == CORE.Input.Keyboard.exitKey && state->keys[i])
            {
                CORE.Window.shouldClose = true;
            }
        }
    }
}


//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// Initialize platform: graphics, inputs and more
int InitPlatform(void)
{
    // TODO: Initialize graphic device: display/window
    // It usually requires setting up the platform display system configuration
    // and connexion with the GPU through some system graphic API
    // raylib uses OpenGL so, platform should create that kind of connection
    // Below example illustrates that process using EGL library
    //----------------------------------------------------------------------------
    if (Windows_Initialize() != 0)
    {
        TRACELOG(LOG_INFO, "WINDOWS: Failed to initialize Windows subsystem!");
        return -1;
    }

    if (Windows_CreateWindow(CORE.Window.title, CORE.Window.screen.width, CORE.Window.screen.height) != 0)
    {
        TRACELOG(LOG_ERROR, "WINDOWS: Failed to create window!");
        return -1;
    }

    // If everything work as expected, we can continue
    CORE.Window.ready = true;
    CORE.Window.render.width = CORE.Window.screen.width;
    CORE.Window.render.height = CORE.Window.screen.height;
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
    TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);

    int x, y, width, height;
    Windows_GetWorkingArea(&x, &y, &width, &height);
    int posX = x + (width - (int)CORE.Window.screen.width) / 2;
    int posY = y + (height - (int)CORE.Window.screen.height) / 2;
    if (posX < x) posX = x;
    if (posY < y) posY = y;
    SetWindowPosition(posX, posY);

    InitTimer();

    CORE.Storage.basePath = GetWorkingDirectory();

    TRACELOG(LOG_INFO, "PLATFORM: CUSTOM: Initialized successfully");

    return 0;
}

// Close platform
void ClosePlatform(void)
{
    Windows_Close();
}

// EOF
