#pragma once

#include <Windows.h>

#include "Keyboard.h"

namespace SomethingMoved
{
    struct InputSnapshot
    {
        bool startPressed = false;
        bool restartPressed = false;
        bool upPressed = false;
        bool downPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;
        bool pausePressed = false;
        bool mutePressed = false;
        bool hintPressed = false;
        bool escapePressed = false;
        bool mousePressed = false;
        bool mouseDown = false;
        bool toggleDebugPressed = false;
        bool advanceDebugPressed = false;
        bool correctDebugPressed = false;
        float mouseWorldX = 0.0f;
        float mouseWorldY = 0.0f;
        bool mouseInClient = false;
    };

    class InputController
    {
    public:
        InputSnapshot Poll();

    private:
        LKeyboard m_keyboard;
    };
}
