#include "InputController.h"

#include <algorithm>

namespace SomethingMoved
{
    InputSnapshot InputController::Poll()
    {
        m_keyboard.GetState();

        const HWND foregroundWindow = GetForegroundWindow();
        DWORD foregroundProcess = 0;
        if (foregroundWindow != nullptr)
        {
            GetWindowThreadProcessId(foregroundWindow, &foregroundProcess);
        }
        const bool inputActive = foregroundProcess == GetCurrentProcessId();

        InputSnapshot input;
        const auto pressed = [this, inputActive](int key)
        {
            // Keep LARC's state current, but normalize actions to the Win32
            // edge. Combining two edge detectors can report one synthetic or
            // very quick press on consecutive frames.
            (void)m_keyboard.TriggerDown(key);
            const bool win32Pressed = (GetAsyncKeyState(key) & 0x0001) != 0;
            return inputActive && win32Pressed;
        };

        input.startPressed = pressed(VK_RETURN) || pressed(VK_SPACE);
        input.restartPressed = pressed('R');
        input.upPressed = pressed(VK_UP) || pressed('W');
        input.downPressed = pressed(VK_DOWN) || pressed('S');
        input.leftPressed = pressed(VK_LEFT) || pressed('A');
        input.rightPressed = pressed(VK_RIGHT) || pressed('D');
        input.pausePressed = pressed('P');
        input.mutePressed = pressed('M');
        input.hintPressed = pressed('H');
        input.escapePressed = pressed(VK_ESCAPE);
        // Mouse buttons are not keyboard keys; polling them through LKeyboard
        // can replay a stale edge after a phase transition.
        const SHORT mouseState = GetAsyncKeyState(VK_LBUTTON);
        input.mousePressed = inputActive && (mouseState & 0x0001) != 0;
        input.mouseDown = inputActive && (mouseState & 0x8000) != 0;
        input.toggleDebugPressed = pressed(VK_F1);
#ifdef _DEBUG
        input.advanceDebugPressed = pressed(VK_F2);
        input.correctDebugPressed = pressed(VK_F3);
#endif

        POINT cursor{};
        HWND window = inputActive ? foregroundWindow : nullptr;
        RECT client{};
        if (window != nullptr && GetCursorPos(&cursor) && ScreenToClient(window, &cursor) &&
            GetClientRect(window, &client))
        {
            const float clientWidth = static_cast<float>(client.right - client.left);
            const float clientHeight = static_cast<float>(client.bottom - client.top);
            const float scale = (std::min)(clientWidth / 1280.0f, clientHeight / 720.0f);
            if (scale > 0.0f)
            {
                const float viewportWidth = 1280.0f * scale;
                const float viewportHeight = 720.0f * scale;
                const float offsetX = (clientWidth - viewportWidth) * 0.5f;
                const float offsetY = (clientHeight - viewportHeight) * 0.5f;
                const float logicalX = (static_cast<float>(cursor.x) - offsetX) / scale;
                const float logicalY = (static_cast<float>(cursor.y) - offsetY) / scale;
                input.mouseInClient = logicalX >= 0.0f && logicalX < 1280.0f &&
                    logicalY >= 0.0f && logicalY < 720.0f;
                input.mouseWorldX = logicalX - 640.0f;
                input.mouseWorldY = 360.0f - logicalY;
            }
        }

        return input;
    }
}
