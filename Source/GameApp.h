#pragma once

#include "AudioController.h"
#include "GameModel.h"
#include "GameRenderer.h"
#include "InputController.h"
#include "Timer.h"
#include "Window.h"

#include <memory>

namespace SomethingMoved
{
    class GameApp
    {
    public:
        GameApp();
        int Run(HINSTANCE instance);

    private:
        void Initialize();
        void Update();
        void Tick();
        void Shutdown();

        GameModel m_model;
        LWindow m_window;
        std::unique_ptr<GameRenderer> m_renderer;
        AudioController m_audio;
        InputController m_input;
        LTimer m_timer;
        int m_hoveredObject = -1;
        bool m_showDebug = false;
        bool m_findClickReady = false;
        int m_menuSelection = 0;
        int m_keyboardObject = 0;
        bool m_keyboardSelectionActive = false;
        bool m_haveMousePosition = false;
        float m_lastMouseWorldX = 0.0f;
        float m_lastMouseWorldY = 0.0f;
        bool m_paused = false;
        float m_visualTime = 0.0f;
    };

    bool RunModelSelfTests();
}
