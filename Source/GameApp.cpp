#include "GameApp.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>

namespace SomethingMoved
{
    GameApp::GameApp() : m_model(), m_window(), m_renderer(), m_input(), m_timer()
    {
        m_timer.SetFixedTimeStep(false);
    }

    int GameApp::Run(HINSTANCE instance)
    {
        return m_window.WinMain(
            instance,
            false,
            [this]() { Initialize(); },
            [this]() { Update(); },
            [this]() { Shutdown(); });
    }

    void GameApp::Initialize()
    {
        // LWindow's internal RoInitializeWrapper goes out of scope before this
        // callback. Re-establish COM for WIC texture loading. LWindow performs
        // the matching CoUninitialize after the shutdown callback.
        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(comResult))
        {
            MessageBoxA(nullptr, "Unable to initialize COM for game assets.",
                "Something Moved", MB_OK | MB_ICONERROR);
            PostQuitMessage(1);
            return;
        }

        // LSpriteRenderer reads LWindow's static dimensions in its constructor.
        // Construct it only after LWindow has loaded gamesettings.xml.
        m_renderer = std::make_unique<GameRenderer>();
        m_renderer->InitializeGameRenderer();
        m_audio.Initialize();
    }

    void GameApp::Update()
    {
        m_timer.Tick([this]() { Tick(); });
        if (m_renderer)
        {
            m_renderer->Render(m_model, m_hoveredObject, m_showDebug,
                m_menuSelection, m_audio.IsMuted(), m_paused, m_visualTime);
        }
    }

    void GameApp::Tick()
    {
        const InputSnapshot input = m_input.Poll();
        const float deltaSeconds = m_timer.GetFrameTime();
        m_visualTime += deltaSeconds;
        m_audio.BeginFrame();

        if (input.escapePressed)
        {
            if (m_paused)
            {
                m_paused = false;
            }
            else if (m_model.GetPhase() == GamePhase::Start)
            {
                PostQuitMessage(0);
            }
            else
            {
                m_model.ReturnToMenu();
                m_menuSelection = 0;
            }
            m_audio.Update(m_model.GetPhase(), m_model.WasLastAnswerCorrect(), input.mutePressed);
            return;
        }

        const GamePhase phaseAtFrameStart = m_model.GetPhase();
        const bool canPause = phaseAtFrameStart == GamePhase::Observe ||
            phaseAtFrameStart == GamePhase::Blackout || phaseAtFrameStart == GamePhase::Find;
        if (input.pausePressed && canPause)
        {
            m_paused = !m_paused;
        }
        if (m_paused)
        {
            m_audio.Update(m_model.GetPhase(), m_model.WasLastAnswerCorrect(), input.mutePressed);
            return;
        }

        if (input.toggleDebugPressed)
        {
            m_showDebug = !m_showDebug;
        }

        GamePhase phase = m_model.GetPhase();
        if (phase == GamePhase::Start)
        {
            bool mouseOverMenu = false;
            if (input.mouseInClient)
            {
                const float screenX = input.mouseWorldX + 640.0f;
                const float screenY = 360.0f - input.mouseWorldY;
                if (screenX >= 55.0f && screenX <= 470.0f)
                {
                    if (screenY >= 310.0f && screenY < 375.0f) { m_menuSelection = 0; mouseOverMenu = true; }
                    else if (screenY >= 375.0f && screenY < 430.0f) { m_menuSelection = 1; mouseOverMenu = true; }
                    else if (screenY >= 430.0f && screenY < 495.0f) { m_menuSelection = 2; mouseOverMenu = true; }
                }
            }
            if (input.upPressed) m_menuSelection = (m_menuSelection + 2) % 3;
            if (input.downPressed) m_menuSelection = (m_menuSelection + 1) % 3;

            if (input.startPressed || (input.mousePressed && mouseOverMenu))
            {
                if (m_menuSelection == 0) m_model.StartNewGame();
                else if (m_menuSelection == 1) m_model.ShowHowTo();
                else PostQuitMessage(0);
            }
        }
        else if (phase == GamePhase::HowTo)
        {
            if (input.startPressed || input.mousePressed)
            {
                m_model.ReturnToMenu();
            }
        }
        else if (phase == GamePhase::Intro && input.startPressed)
        {
            m_model.BeginTutorial();
            m_keyboardObject = 0;
        }
        else if ((phase == GamePhase::Victory || phase == GamePhase::GameOver) &&
            (input.startPressed || input.restartPressed))
        {
            m_model.StartNewGame();
        }

        const bool keyboardNavigation = input.leftPressed || input.upPressed ||
            input.rightPressed || input.downPressed;
        bool mouseMoved = false;
        if (input.mouseInClient)
        {
            mouseMoved = !m_haveMousePosition ||
                std::fabs(input.mouseWorldX - m_lastMouseWorldX) > 0.5f ||
                std::fabs(input.mouseWorldY - m_lastMouseWorldY) > 0.5f;
            m_lastMouseWorldX = input.mouseWorldX;
            m_lastMouseWorldY = input.mouseWorldY;
            m_haveMousePosition = true;
        }
        if (m_model.GetPhase() == GamePhase::Find && keyboardNavigation)
        {
            const int objectCount = static_cast<int>(m_model.GetObjects().size());
            const int direction = (input.leftPressed || input.upPressed) ? -1 : 1;
            m_keyboardObject = (m_keyboardObject + objectCount + direction) % objectCount;
            m_keyboardSelectionActive = true;
            m_hoveredObject = m_keyboardObject;
        }
        else if (m_model.GetPhase() == GamePhase::Find && m_keyboardSelectionActive && !mouseMoved)
        {
            m_hoveredObject = m_keyboardObject;
        }
        else if (input.mouseInClient)
        {
            m_keyboardSelectionActive = false;
            m_hoveredObject = m_model.HitTest(input.mouseWorldX, input.mouseWorldY);
        }
        else if (m_model.GetPhase() != GamePhase::Find)
        {
            m_keyboardSelectionActive = false;
            m_hoveredObject = -1;
        }

        if (m_model.GetPhase() != GamePhase::Find)
        {
            m_findClickReady = false;
        }
        else if (!m_findClickReady)
        {
            // A click made during blackout must be fully released before the
            // Find phase can accept a selection.
            m_findClickReady = !input.mouseDown && !input.mousePressed;
        }
        else if (input.mousePressed && m_hoveredObject >= 0)
        {
            m_model.SubmitSelection(static_cast<std::size_t>(m_hoveredObject));
            m_findClickReady = false;
        }
        else if (input.startPressed && m_hoveredObject >= 0)
        {
            m_model.SubmitSelection(static_cast<std::size_t>(m_hoveredObject));
            m_findClickReady = false;
        }

        if (input.hintPressed && m_model.GetPhase() == GamePhase::Find)
        {
            m_model.UseHint();
        }

#ifdef _DEBUG
        if (input.correctDebugPressed && m_model.GetPhase() == GamePhase::Find &&
            m_model.GetChangeRecord().valid)
        {
            m_model.SubmitSelection(m_model.GetChangeRecord().objectIndex);
        }
        else if (input.advanceDebugPressed)
        {
            // Advance only after processing input for the phase in which it was
            // sampled, so a blackout click cannot leak into Find.
            m_model.DebugAdvancePhase();
        }
#endif

        m_model.Update(deltaSeconds);
        m_audio.Update(m_model.GetPhase(), m_model.WasLastAnswerCorrect(), input.mutePressed);
    }

    void GameApp::Shutdown()
    {
        m_audio.Shutdown();
        m_renderer.reset();
    }

    bool RunModelSelfTests()
    {
        // A successful model test must also prove that a clean checkout has
        // every runtime file referenced by the XML and renderer.
        static constexpr std::array<const wchar_t*, 15> RequiredFiles = {
            L"Media\\xml\\gamesettings.xml",
            L"Media\\Fonts\\courier.spritefont",
            L"Media\\Audio\\rain.wav",
            L"Media\\Audio\\blackout.wav",
            L"Media\\Audio\\correct.wav",
            L"Media\\Audio\\incorrect.wav",
            L"Media\\Textures\\BedroomRoom\\normal.png",
            L"Media\\Textures\\BedroomRoom\\teddy_missing.png",
            L"Media\\Textures\\BedroomRoom\\lamp_off.png",
            L"Media\\Textures\\BedroomRoom\\portrait_missing.png",
            L"Media\\Textures\\BedroomRoom\\clock_changed.png",
            L"Media\\Textures\\BedroomRoom\\phone_missing.png",
            L"Media\\Textures\\BedroomRoom\\books_missing.png",
            L"Media\\Textures\\BedroomRoom\\candle_out.png",
            L"Media\\Textures\\BedroomRoom\\wardrobe_open.png"
        };
        for (const wchar_t* file : RequiredFiles)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(file, error) || error ||
                std::filesystem::file_size(file, error) == 0 || error)
            {
                return false;
            }
        }

        GameModel victoryModel(0x4210u);
        if (victoryModel.GetPhase() != GamePhase::Start)
        {
            return false;
        }

        victoryModel.ShowHowTo();
        if (victoryModel.GetPhase() != GamePhase::HowTo)
        {
            return false;
        }
        victoryModel.ReturnToMenu();
        if (victoryModel.GetPhase() != GamePhase::Start)
        {
            return false;
        }

        victoryModel.StartNewGame();
        if (victoryModel.GetPhase() != GamePhase::Intro)
        {
            return false;
        }
        victoryModel.BeginTutorial();
        {
            const auto& catalog = victoryModel.GetObjects();
            if (catalog.size() != 8)
            {
                return false;
            }
            std::array<bool, 8> usedVariants{};
            for (const RoomObject& object : catalog)
            {
                if (object.name.empty() || object.changeDescription.empty() ||
                    object.observationHint.empty() || object.spriteIndex >= usedVariants.size() ||
                    usedVariants[object.spriteIndex] || object.hitWidth <= 0.0f || object.hitHeight <= 0.0f)
                {
                    return false;
                }
                usedVariants[object.spriteIndex] = true;
            }
        }
        for (int expectedRound = 1; expectedRound <= GameModel::TotalRounds; ++expectedRound)
        {
            if (victoryModel.GetPhase() != GamePhase::Observe || victoryModel.GetRound() != expectedRound)
            {
                return false;
            }

            victoryModel.Update(GameModel::ObserveDuration + 0.1f);
            if (victoryModel.GetPhase() != GamePhase::Blackout ||
                !victoryModel.GetChangeRecord().valid || victoryModel.CountChangedObjects() != 1)
            {
                return false;
            }
            const ChangeRecord& generated = victoryModel.GetChangeRecord();
            if ((expectedRound == 1 && generated.objectIndex != 0) ||
                (expectedRound == GameModel::TotalRounds && generated.objectIndex != 7) ||
                victoryModel.GetObjects()[generated.objectIndex].changeDescription.empty() ||
                victoryModel.GetObjects()[generated.objectIndex].difficultyTier >
                    victoryModel.GetDifficultyTier())
            {
                return false;
            }

            const int mistakesBeforeBlackoutClick = victoryModel.GetMistakes();
            victoryModel.SubmitSelection(0);
            if (victoryModel.GetPhase() != GamePhase::Blackout ||
                victoryModel.GetMistakes() != mistakesBeforeBlackoutClick)
            {
                return false;
            }

            const ChangeRecord change = victoryModel.GetChangeRecord();
            const RoomObject& changedObject = victoryModel.GetObjects()[change.objectIndex];
            if (victoryModel.HitTest(changedObject.currentState.x, changedObject.currentState.y) !=
                static_cast<int>(change.objectIndex))
            {
                return false;
            }

            victoryModel.Update(GameModel::BlackoutDuration + 0.1f);
            if (victoryModel.GetPhase() != GamePhase::Find)
            {
                return false;
            }

            victoryModel.SubmitSelection(change.objectIndex);
            if (victoryModel.GetPhase() != GamePhase::Feedback ||
                !victoryModel.WasLastAnswerCorrect())
            {
                return false;
            }
            victoryModel.Update(GameModel::FeedbackDuration + 0.1f);
        }

        if (victoryModel.GetPhase() != GamePhase::Victory || victoryModel.GetScore() <= 0 ||
            victoryModel.GetCorrectAnswers() != GameModel::TotalRounds)
        {
            return false;
        }

        victoryModel.StartNewGame();
        if (victoryModel.GetPhase() != GamePhase::Intro || victoryModel.GetRound() != 1 ||
            victoryModel.GetScore() != 0 || victoryModel.GetMistakes() != 0)
        {
            return false;
        }

        GameModel failureModel(0xDEADu);
        failureModel.StartNewGame();
        failureModel.BeginTutorial();
        for (int mistake = 0; mistake < GameModel::MaximumMistakes; ++mistake)
        {
            failureModel.Update(GameModel::ObserveDuration + 0.1f);
            failureModel.Update(GameModel::BlackoutDuration + 0.1f);
            const std::size_t wrongIndex = (failureModel.GetChangeRecord().objectIndex + 1) %
                failureModel.GetObjects().size();
            failureModel.SubmitSelection(wrongIndex);
            failureModel.Update(GameModel::FeedbackDuration + 0.1f);
        }

        if (failureModel.GetPhase() != GamePhase::GameOver ||
            failureModel.GetMistakes() != GameModel::MaximumMistakes)
        {
            return false;
        }
        failureModel.StartNewGame();
        if (failureModel.GetPhase() != GamePhase::Intro || failureModel.GetRound() != 1 ||
            failureModel.GetMistakes() != 0 || failureModel.GetScore() != 0)
        {
            return false;
        }
        failureModel.ReturnToMenu();
        if (failureModel.GetPhase() != GamePhase::Start)
        {
            return false;
        }

        GameModel timeoutModel(0xBEEFu);
        timeoutModel.StartNewGame();
        timeoutModel.BeginTutorial();
        timeoutModel.Update(GameModel::ObserveDuration + 0.1f);
        timeoutModel.Update(GameModel::BlackoutDuration + 0.1f);
        timeoutModel.Update(GameModel::FindDuration + 0.1f);
        if (timeoutModel.GetPhase() != GamePhase::Feedback || timeoutModel.GetMistakes() != 1 ||
            !timeoutModel.WasLastAnswerTimedOut())
        {
            return false;
        }

        // Hint system: 2 hints per round, use them or lose them. Each hint
        // dims one randomly chosen wrong object and costs 100 points,
        // floored at zero, and never dims the changed object.
        {
            GameModel hintModel(0x5EEDu);
            hintModel.StartNewGame();
            hintModel.BeginTutorial();
            hintModel.Update(GameModel::ObserveDuration + 0.1f);
            hintModel.Update(GameModel::BlackoutDuration + 0.1f);
            if (hintModel.GetPhase() != GamePhase::Find ||
                hintModel.GetHintsRemaining() != GameModel::HintsPerRound ||
                !hintModel.GetDimmedObjects().empty())
            {
                return false;
            }
            const std::size_t changedIndex = hintModel.GetChangeRecord().objectIndex;
            if (!hintModel.UseHint() ||
                hintModel.GetHintsRemaining() != GameModel::HintsPerRound - 1 ||
                hintModel.GetDimmedObjects().size() != 1 ||
                hintModel.GetDimmedObjects()[0] == changedIndex)
            {
                return false;
            }
            if (!hintModel.UseHint() || hintModel.GetHintsRemaining() != 0 ||
                hintModel.GetDimmedObjects().size() != 2)
            {
                return false;
            }
            if (hintModel.UseHint())
            {
                return false;
            }
            hintModel.SubmitSelection(changedIndex);
            if (hintModel.GetPhase() != GamePhase::Feedback || !hintModel.WasLastAnswerCorrect())
            {
                return false;
            }
            hintModel.Update(GameModel::FeedbackDuration + 0.1f);
            hintModel.Update(GameModel::ObserveDuration + 0.1f);
            hintModel.Update(GameModel::BlackoutDuration + 0.1f);
            if (hintModel.GetPhase() != GamePhase::Find ||
                hintModel.GetHintsRemaining() != GameModel::HintsPerRound ||
                !hintModel.GetDimmedObjects().empty())
            {
                return false;
            }
        }

        // Decision Brief validation: 1,000 deterministic ten-round games are
        // 10,000 authored anomaly selections. Every selection must change
        // exactly one object, be clickable in-frame even when invisible, and
        // never immediately repeat the previous object.
        for (std::uint32_t seed = 1; seed <= 1000; ++seed)
        {
            GameModel validationModel(seed);
            validationModel.StartNewGame();
            validationModel.BeginTutorial();
            std::size_t previousObject = static_cast<std::size_t>(-1);
            for (int round = 0; round < GameModel::TotalRounds; ++round)
            {
                validationModel.Update(GameModel::ObserveDuration + 0.1f);
                const ChangeRecord& change = validationModel.GetChangeRecord();
                if (validationModel.GetPhase() != GamePhase::Blackout || !change.valid ||
                    validationModel.CountChangedObjects() != 1 || change.objectIndex == previousObject)
                {
                    return false;
                }

                const RoomObject& object = validationModel.GetObjects()[change.objectIndex];
                const float halfWidth = object.hitWidth * 0.5f;
                const float halfHeight = object.hitHeight * 0.5f;
                if (object.currentState.x - halfWidth < -640.0f ||
                    object.currentState.x + halfWidth > 640.0f ||
                    object.currentState.y - halfHeight < -360.0f ||
                    object.currentState.y + halfHeight > 360.0f ||
                    validationModel.HitTest(object.currentState.x, object.currentState.y) !=
                        static_cast<int>(change.objectIndex))
                {
                    return false;
                }

                previousObject = change.objectIndex;
                validationModel.Update(GameModel::BlackoutDuration + 0.1f);
                validationModel.SubmitSelection(change.objectIndex);
                validationModel.Update(GameModel::FeedbackDuration + 0.1f);
            }
            if (validationModel.GetPhase() != GamePhase::Victory)
            {
                return false;
            }
        }
        return true;
    }
}
