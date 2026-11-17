#include "GameRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

namespace SomethingMoved
{
    namespace
    {
        DirectX::XMVECTORF32 Color(float red, float green, float blue, float alpha = 1.0f)
        {
            return DirectX::XMVECTORF32{ red, green, blue, alpha };
        }
    }

    GameRenderer::GameRenderer() : LSpriteRenderer(eSpriteMode::Batched2D)
    {
    }

    void GameRenderer::InitializeGameRenderer()
    {
        // LSpriteRenderer constructs its camera before LWindow loads the XML
        // settings, so its initial orthographic dimensions are zero. Rebuild
        // the camera here, after the window has loaded the real dimensions.
        m_pCamera->SetOrthographic(1280.0f, 720.0f, 0.1f, 20000.0f);
        m_pCamera->MoveTo(Vector3::Zero);

        Initialize(SpriteId::Count);
        SetBgColor(Color(0.006f, 0.008f, 0.014f));

        BeginResourceUpload();
        Load(SpriteId::RoomNormal, "room_normal");
        Load(SpriteId::RoomTeddyMissing, "room_teddy_missing");
        Load(SpriteId::RoomLampOff, "room_lamp_off");
        Load(SpriteId::RoomPortraitMissing, "room_portrait_missing");
        Load(SpriteId::RoomClockChanged, "room_clock_changed");
        Load(SpriteId::RoomPhoneMissing, "room_phone_missing");
        Load(SpriteId::RoomBooksMissing, "room_books_missing");
        Load(SpriteId::RoomCandleOut, "room_candle_out");
        Load(SpriteId::RoomWardrobeOpen, "room_wardrobe_open");
        Load(SpriteId::Solid, "solid");
        Load(SpriteId::Glow, "glow");
        EndResourceUpload();
    }

    void GameRenderer::Render(const GameModel& model, int hoveredObject, bool showDebug,
        int menuSelection, bool muted, bool paused, float visualTime)
    {
        m_visualTime = visualTime;
        BeginFrame();
        DrawRoom(model, hoveredObject);
        DrawHud(model, muted);
        DrawOverlay(model, menuSelection, muted, paused);
        if (showDebug)
        {
            DrawDebug(model, hoveredObject);
        }
        EndFrame();
    }

    void GameRenderer::DrawRoom(const GameModel& model, int hoveredObject)
    {
        const float brightness = model.GetRoomBrightness();
        SpriteId roomSprite = SpriteId::RoomNormal;
        // Keep pre-change imagery through the entire blackout so an electrical
        // flicker cannot reveal the answer. The authored anomaly appears only
        // when the Find phase begins (and remains visible through feedback).
        if (model.GetChangeRecord().valid && model.GetPhase() != GamePhase::Blackout)
        {
            const auto& objects = model.GetObjects();
            const std::size_t index = model.GetChangeRecord().objectIndex;
            if (index < objects.size() && objects[index].spriteIndex < 8)
            {
                roomSprite = static_cast<SpriteId>(
                    static_cast<int>(SpriteId::RoomTeddyMissing) +
                    static_cast<int>(objects[index].spriteIndex));
            }
        }

        LSpriteDesc2D background(static_cast<UINT>(roomSprite), Vector2::Zero);
        background.m_f4Tint = DirectX::XMFLOAT4(brightness, brightness, brightness, 1.0f);
        Draw(&background);
        DrawAtmosphere(model);

        const auto& objects = model.GetObjects();
        const float frameSeconds = std::clamp(m_visualTime - m_lastVisualTime, 0.0f, 0.1f);
        m_lastVisualTime = m_visualTime;
        for (std::size_t index = 0; index < objects.size() && index < m_hoverAlpha.size(); ++index)
        {
            const float target = model.GetPhase() == GamePhase::Find &&
                hoveredObject == static_cast<int>(index) ? 1.0f : 0.0f;
            m_hoverAlpha[index] += (target - m_hoverAlpha[index]) * std::min(1.0f, frameSeconds * 8.0f);
        }

        // Hints dim one wrong object each: a dark panel over its hit rect so
        // the player can rule it out without the answer being revealed.
        if (model.GetPhase() == GamePhase::Find)
        {
            for (const std::size_t dimmedIndex : model.GetDimmedObjects())
            {
                if (dimmedIndex >= objects.size())
                {
                    continue;
                }
                const RoomObject& dimmed = objects[dimmedIndex];
                const float dimWidth = dimmed.hitWidth;
                const float dimHeight = dimmed.hitHeight;
                DrawPanel(dimmed.currentState.x + 640.0f - dimWidth * 0.5f,
                    360.0f - dimmed.currentState.y - dimHeight * 0.5f,
                    dimWidth, dimHeight,
                    DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), 0.55f);
            }
        }

        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            const RoomObject& object = objects[index];
            const ObjectState& state = object.currentState;
            const float hoverAmount = index < m_hoverAlpha.size() ? m_hoverAlpha[index] : 0.0f;
            const bool hovered = hoverAmount > 0.01f;

            if (hovered)
            {
                const float pulse = 0.72f + 0.18f * std::sin(m_visualTime * 7.0f);
                LSpriteDesc2D glow(static_cast<UINT>(SpriteId::Glow), Vector2(state.x, state.y));
                glow.m_fXScale = std::max(0.16f, object.hitWidth / 610.0f);
                glow.m_fYScale = std::max(0.16f, object.hitHeight / 610.0f);
                glow.m_f4Tint = state.visible
                    ? DirectX::XMFLOAT4(0.96f, 0.60f, 0.22f, 1.0f)
                    : DirectX::XMFLOAT4(0.34f, 0.62f, 0.90f, 1.0f);
                glow.m_fAlpha = (state.visible ? 0.22f : 0.10f) * pulse * hoverAmount;
                Draw(&glow);
            }

            if (model.GetPhase() == GamePhase::Feedback && model.GetChangeRecord().valid &&
                model.GetChangeRecord().objectIndex == index)
            {
                LSpriteDesc2D resultGlow(static_cast<UINT>(SpriteId::Glow), Vector2(state.x, state.y));
                resultGlow.m_fXScale = std::max(0.17f, object.hitWidth / 570.0f);
                resultGlow.m_fYScale = std::max(0.17f, object.hitHeight / 570.0f);
                resultGlow.m_f4Tint = model.WasLastAnswerCorrect()
                    ? DirectX::XMFLOAT4(0.28f, 1.0f, 0.56f, 1.0f)
                    : DirectX::XMFLOAT4(1.0f, 0.20f, 0.18f, 1.0f);
                resultGlow.m_fAlpha = 0.13f + 0.05f * std::sin(m_visualTime * 8.0f);
                Draw(&resultGlow);
            }
        }

        DrawVignette();

        if (model.GetPhase() == GamePhase::Blackout)
        {
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.0f, 0.0f, 0.02f, 1.0f),
                1.0f - brightness);
        }
    }

    void GameRenderer::DrawAtmosphere(const GameModel& model)
    {
        const float brightness = model.GetRoomBrightness();
        const float tension = static_cast<float>(model.GetDifficultyTier()) / 2.0f;

        LSpriteDesc2D warm(static_cast<UINT>(SpriteId::Glow), Vector2(-485.0f, 5.0f));
        warm.m_fXScale = 0.50f;
        warm.m_fYScale = 0.40f;
        warm.m_f4Tint = DirectX::XMFLOAT4(1.0f, 0.48f, 0.16f, 1.0f);
        warm.m_fAlpha = (0.12f - tension * 0.035f + 0.010f * std::sin(m_visualTime * 4.7f)) * brightness;
        Draw(&warm);

        LSpriteDesc2D moon(static_cast<UINT>(SpriteId::Glow), Vector2(350.0f, 95.0f));
        moon.m_fXScale = 0.58f;
        moon.m_fYScale = 0.50f;
        moon.m_f4Tint = DirectX::XMFLOAT4(0.22f, 0.48f, 0.92f, 1.0f);
        moon.m_fAlpha = (0.08f + tension * 0.06f) * brightness;
        Draw(&moon);

        if (model.GetPhase() != GamePhase::Blackout)
        {
            DrawRain();
        }

        const float lightningCycle = std::fmod(m_visualTime, 19.0f - tension * 6.0f);
        if (lightningCycle > 0.10f && lightningCycle < 0.24f &&
            model.GetPhase() != GamePhase::Blackout)
        {
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.48f, 0.68f, 1.0f, 1.0f),
                (0.055f + tension * 0.035f) * brightness);
        }

        // Dust motes drift only through the warm light so they read as room
        // atmosphere rather than full-screen noise.
        for (int index = 0; index < 14; ++index)
        {
            const float seed = static_cast<float>(index) * 19.371f + m_visualTime * 9.0f;
            const float x = 35.0f + std::fmod(std::fabs(std::sin(seed * 1.73f)) * 500.0f, 500.0f);
            const float y = 235.0f + std::fmod(std::fabs(std::cos(seed * 2.11f)) * 390.0f, 390.0f);
            DrawPanel(x, y, 2.0f, 2.0f,
                DirectX::XMFLOAT4(0.92f, 0.72f, 0.42f, 1.0f), 0.055f * brightness);
        }
    }

    void GameRenderer::DrawRain()
    {
        for (int index = 0; index < 22; ++index)
        {
            const float lane = static_cast<float>((index * 47) % 260);
            const float fall = std::fmod(m_visualTime * (115.0f + index * 3.0f) + index * 31.0f, 285.0f);
            const float x = 845.0f + lane;
            const float y = 95.0f + fall;
            if (y < 390.0f)
            {
                DrawPanel(x, y, 1.0f, 15.0f,
                    DirectX::XMFLOAT4(0.45f, 0.66f, 0.90f, 1.0f), 0.14f);
            }
        }
    }

    void GameRenderer::DrawVignette()
    {
        DrawPanel(0.0f, 0.0f, 1280.0f, 34.0f,
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), 0.42f);
        DrawPanel(0.0f, 686.0f, 1280.0f, 34.0f,
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), 0.48f);
        DrawPanel(0.0f, 0.0f, 28.0f, 720.0f,
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), 0.38f);
        DrawPanel(1252.0f, 0.0f, 28.0f, 720.0f,
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), 0.38f);
    }

    void GameRenderer::DrawHud(const GameModel& model, bool muted)
    {
        const GamePhase phase = model.GetPhase();
        if (phase == GamePhase::Start || phase == GamePhase::HowTo || phase == GamePhase::Intro ||
            phase == GamePhase::Victory || phase == GamePhase::GameOver)
        {
            return;
        }

        DrawPanel(18.0f, 15.0f, 1244.0f, 48.0f,
            DirectX::XMFLOAT4(0.012f, 0.017f, 0.027f, 1.0f), 0.80f);
        DrawPanel(18.0f, 62.0f, 1244.0f, 1.0f,
            DirectX::XMFLOAT4(0.58f, 0.43f, 0.24f, 1.0f), 0.42f);

        char left[64]{};
        char timer[32]{};
        char score[32]{};
        char mistakes[32]{};
        const char* compactPhase = phase == GamePhase::Find ? "FIND" : GameModel::PhaseName(phase);
        std::snprintf(left, sizeof(left), "ROUND %02d/%02d", model.GetRound(), GameModel::TotalRounds);
        std::snprintf(timer, sizeof(timer), "%02d",
            static_cast<int>(std::ceil(model.GetPhaseTimeRemaining())));
        std::snprintf(score, sizeof(score), "SCORE %04d", model.GetScore());
        std::snprintf(mistakes, sizeof(mistakes), "MISTAKES %d/%d",
            model.GetMistakes(), GameModel::MaximumMistakes);

        DrawText(left, Vector2(34.0f, 29.0f), Color(0.72f, 0.82f, 0.90f), 0.56f);
        DrawText(compactPhase, Vector2(430.0f, 29.0f),
            phase == GamePhase::Find ? Color(0.95f, 0.76f, 0.34f) : Color(0.72f, 0.82f, 0.90f));
        DrawText(timer, Vector2(690.0f, 26.0f),
            model.GetPhaseTimeRemaining() <= 5.0f ? Color(1.0f, 0.44f, 0.36f) : Color(0.88f, 0.90f, 0.88f), 0.72f);
        DrawText(score, Vector2(820.0f, 29.0f), Color(0.80f, 0.76f, 0.60f), 0.56f);
        DrawText(mistakes, Vector2(1050.0f, 29.0f), Color(0.82f, 0.68f, 0.68f), 0.56f);
        DrawText(muted ? "M  SOUND OFF" : "M  SOUND ON", Vector2(1110.0f, 690.0f),
            muted ? Color(0.62f, 0.58f, 0.58f) : Color(0.48f, 0.62f, 0.68f), 0.43f);
        if (phase == GamePhase::Observe || phase == GamePhase::Blackout || phase == GamePhase::Find)
        {
            DrawText("P  PAUSE", Vector2(995.0f, 690.0f), Color(0.42f, 0.52f, 0.58f), 0.43f);
        }
        if (phase == GamePhase::Find)
        {
            char hints[32]{};
            std::snprintf(hints, sizeof(hints), "H  HINT x%d", model.GetHintsRemaining());
            DrawText(hints, Vector2(880.0f, 690.0f), Color(0.48f, 0.62f, 0.68f), 0.43f);
        }

        if (model.IsTutorialRound() && phase == GamePhase::Observe)
        {
            DrawPanel(330.0f, 625.0f, 620.0f, 52.0f,
                DirectX::XMFLOAT4(0.015f, 0.025f, 0.035f, 1.0f), 0.82f);
            DrawText("TUTORIAL  Study the teddy bear on the bed.", Vector2(395.0f, 642.0f),
                Color(0.90f, 0.72f, 0.38f), 0.48f);
        }
        else if (model.IsTutorialRound() && phase == GamePhase::Find)
        {
            DrawPanel(330.0f, 625.0f, 620.0f, 52.0f,
                DirectX::XMFLOAT4(0.015f, 0.025f, 0.035f, 1.0f), 0.82f);
            DrawText("Click the place where the teddy bear was.", Vector2(405.0f, 642.0f),
                Color(0.90f, 0.72f, 0.38f), 0.48f);
        }
    }

    void GameRenderer::DrawOverlay(const GameModel& model, int menuSelection, bool muted, bool paused)
    {
        if (paused)
        {
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.003f, 0.006f, 0.012f, 1.0f), 0.76f);
            DrawText("PAUSED", Vector2(545.0f, 285.0f), Color(0.88f, 0.90f, 0.92f), 0.92f);
            DrawText("P OR ESC   RETURN TO THE ROOM", Vector2(430.0f, 370.0f),
                Color(0.94f, 0.70f, 0.34f), 0.50f);
            DrawText("M   TOGGLE SOUND", Vector2(500.0f, 420.0f),
                Color(0.56f, 0.64f, 0.68f), 0.44f);
            return;
        }
        switch (model.GetPhase())
        {
        case GamePhase::Start:
        {
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.005f, 0.008f, 0.015f, 1.0f), 0.40f);
            DrawPanel(0.0f, 0.0f, 520.0f, 720.0f,
                DirectX::XMFLOAT4(0.006f, 0.010f, 0.017f, 1.0f), 0.72f);
            DrawText("S O M E T H I N G", Vector2(70.0f, 132.0f),
                Color(0.84f, 0.88f, 0.90f), 0.82f);
            DrawText("M O V E D", Vector2(70.0f, 184.0f),
                Color(0.94f, 0.66f, 0.30f), 0.96f);
            DrawText("WHEN THE LIGHTS FAIL, REMEMBER.", Vector2(74.0f, 250.0f),
                Color(0.54f, 0.62f, 0.68f), 0.46f);
            const float pulse = 0.84f + 0.16f * std::sin(m_visualTime * 3.0f);
            const auto selectedColor = Color(1.0f, 0.66f * pulse, 0.28f);
            const auto normalColor = Color(0.58f, 0.66f, 0.72f);
            DrawText(menuSelection == 0 ? ">  START" : "   START", Vector2(90.0f, 340.0f),
                menuSelection == 0 ? selectedColor : normalColor, 0.70f);
            DrawText(menuSelection == 1 ? ">  HOW TO PLAY" : "   HOW TO PLAY", Vector2(90.0f, 395.0f),
                menuSelection == 1 ? selectedColor : normalColor, 0.70f);
            DrawText(menuSelection == 2 ? ">  EXIT" : "   EXIT", Vector2(90.0f, 450.0f),
                menuSelection == 2 ? selectedColor : normalColor, 0.70f);
            DrawText("ARROWS / W S   SELECT     ENTER   CONFIRM", Vector2(74.0f, 615.0f),
                Color(0.40f, 0.50f, 0.56f), 0.39f);
            DrawText(muted ? "M   SOUND OFF" : "M   SOUND ON", Vector2(74.0f, 652.0f),
                Color(0.45f, 0.56f, 0.60f), 0.39f);
            break;
        }

        case GamePhase::Intro:
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.003f, 0.006f, 0.012f, 1.0f), 0.70f);
            DrawPanel(230.0f, 150.0f, 820.0f, 410.0f,
                DirectX::XMFLOAT4(0.012f, 0.020f, 0.030f, 1.0f), 0.86f);
            DrawText("THE FIRST NIGHT", Vector2(430.0f, 205.0f),
                Color(0.90f, 0.74f, 0.46f), 0.86f);
            DrawText("The door locked when the rain began.", Vector2(365.0f, 300.0f),
                Color(0.72f, 0.78f, 0.80f), 0.54f);
            DrawText("A note beneath it reads:", Vector2(430.0f, 350.0f),
                Color(0.58f, 0.66f, 0.70f), 0.48f);
            DrawText("REMEMBER THE ROOM.  TEN TIMES.", Vector2(355.0f, 415.0f),
                Color(0.94f, 0.60f, 0.32f), 0.62f);
            DrawText("ENTER   BEGIN", Vector2(505.0f, 505.0f),
                Color(0.88f, 0.74f, 0.46f), 0.50f);
            break;

        case GamePhase::HowTo:
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.005f, 0.010f, 0.018f, 1.0f), 0.78f);
            DrawPanel(190.0f, 78.0f, 900.0f, 570.0f,
                DirectX::XMFLOAT4(0.014f, 0.025f, 0.038f, 1.0f), 0.88f);
            DrawText("HOW TO PLAY", Vector2(235.0f, 120.0f), Color(0.86f, 0.90f, 0.92f), 0.82f);
            DrawText("1  OBSERVE", Vector2(245.0f, 215.0f), Color(0.94f, 0.68f, 0.30f), 0.62f);
            DrawText("Study each object and where it belongs.", Vector2(245.0f, 260.0f),
                Color(0.66f, 0.73f, 0.78f), 0.48f);
            DrawText("2  BLACKOUT", Vector2(245.0f, 335.0f), Color(0.94f, 0.68f, 0.30f), 0.62f);
            DrawText("One real object changes in the darkness.", Vector2(245.0f, 380.0f),
                Color(0.66f, 0.73f, 0.78f), 0.48f);
            DrawText("3  FIND", Vector2(245.0f, 455.0f), Color(0.94f, 0.68f, 0.30f), 0.62f);
            DrawText("Click it, or use arrows + Enter, before time runs out.", Vector2(245.0f, 500.0f),
                Color(0.66f, 0.73f, 0.78f), 0.48f);
            DrawPanel(760.0f, 190.0f, 1.0f, 330.0f,
                DirectX::XMFLOAT4(0.45f, 0.39f, 0.28f, 1.0f), 0.46f);
            DrawText("10 ROUNDS", Vector2(815.0f, 255.0f), Color(0.76f, 0.70f, 0.56f), 0.60f);
            DrawText("TO ESCAPE", Vector2(815.0f, 295.0f), Color(0.76f, 0.70f, 0.56f), 0.50f);
            DrawText("3 MISTAKES", Vector2(815.0f, 385.0f), Color(0.76f, 0.48f, 0.46f), 0.60f);
            DrawText("AND THE DOOR STAYS SHUT", Vector2(815.0f, 430.0f),
                Color(0.64f, 0.56f, 0.56f), 0.39f);
            DrawText("ENTER OR ESC   BACK", Vector2(485.0f, 595.0f),
                Color(0.90f, 0.68f, 0.34f), 0.50f);
            break;

        case GamePhase::Blackout:
            if (model.GetRoomBrightness() < 0.20f)
            {
                DrawText("DON'T LOOK AWAY", Vector2(535.0f, 350.0f),
                    Color(0.34f, 0.38f, 0.44f), 0.58f);
            }
            if (model.IsFinalRound())
            {
                DrawText("IT KNOWS YOU ARE WATCHING", Vector2(435.0f, 410.0f),
                    Color(0.48f, 0.22f, 0.24f), 0.48f);
            }
            break;

        case GamePhase::Feedback:
            DrawPanel(375.0f, 275.0f, 530.0f, 155.0f,
                model.WasLastAnswerCorrect()
                    ? DirectX::XMFLOAT4(0.02f, 0.12f, 0.09f, 1.0f)
                    : DirectX::XMFLOAT4(0.18f, 0.025f, 0.025f, 1.0f),
                0.91f);
            DrawText(model.WasLastAnswerCorrect() ? "YOU SAW IT." :
                (model.WasLastAnswerTimedOut() ? "TOO LATE." : "IT WASN'T THAT."),
                Vector2(model.WasLastAnswerCorrect() ? 520.0f :
                    (model.WasLastAnswerTimedOut() ? 535.0f : 485.0f), 305.0f),
                model.WasLastAnswerCorrect() ? Color(0.50f, 1.0f, 0.72f) : Color(1.0f, 0.50f, 0.48f), 0.72f);
            if (model.GetChangeRecord().valid)
            {
                const auto& changedObject = model.GetObjects()[model.GetChangeRecord().objectIndex];
                char objectMessage[96]{};
                std::snprintf(objectMessage, sizeof(objectMessage), "Object: %s", changedObject.name.c_str());
                DrawText(objectMessage, Vector2(455.0f, 370.0f), Color(0.76f, 0.80f, 0.82f), 0.46f);
                DrawText(changedObject.changeDescription.c_str(), Vector2(455.0f, 400.0f),
                    Color(0.76f, 0.80f, 0.82f), 0.46f);
            }
            break;

        case GamePhase::Victory:
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.01f, 0.08f, 0.06f, 1.0f), 0.82f);
            DrawText("YOU REMEMBERED EVERYTHING.", Vector2(365.0f, 260.0f),
                Color(0.54f, 1.0f, 0.76f), 0.74f);
            DrawText("The door unlocks.", Vector2(490.0f, 340.0f),
                Color(0.72f, 0.82f, 0.82f), 0.58f);
            {
                char result[96]{};
                std::snprintf(result, sizeof(result), "SCORE %04d     DETECTED %d/%d",
                    model.GetScore(), model.GetCorrectAnswers(), GameModel::TotalRounds);
                DrawText(result, Vector2(405.0f, 390.0f), Color(0.62f, 0.72f, 0.72f), 0.48f);
            }
            DrawText("R OR ENTER   PLAY AGAIN", Vector2(445.0f, 430.0f),
                Color(0.94f, 0.74f, 0.38f), 0.52f);
            break;

        case GamePhase::GameOver:
            DrawPanel(0.0f, 0.0f, 1280.0f, 720.0f,
                DirectX::XMFLOAT4(0.10f, 0.005f, 0.008f, 1.0f), 0.84f);
            DrawText("THE ROOM REMEMBERS YOU.", Vector2(390.0f, 250.0f),
                Color(1.0f, 0.48f, 0.44f), 0.76f);
            DrawText("Three mistakes. The door stays shut.", Vector2(400.0f, 350.0f),
                Color(0.76f, 0.70f, 0.70f), 0.54f);
            {
                char result[96]{};
                std::snprintf(result, sizeof(result), "SCORE %04d     DETECTED %d/%d",
                    model.GetScore(), model.GetCorrectAnswers(), model.GetRound());
                DrawText(result, Vector2(420.0f, 405.0f), Color(0.68f, 0.62f, 0.62f), 0.46f);
            }
            DrawText("R OR ENTER   TRY AGAIN", Vector2(455.0f, 445.0f),
                Color(0.94f, 0.74f, 0.38f), 0.52f);
            break;

        default:
            break;
        }
    }

    void GameRenderer::DrawDebug(const GameModel& model, int hoveredObject)
    {
        DrawPanel(18.0f, 646.0f, 1244.0f, 52.0f,
            DirectX::XMFLOAT4(0.02f, 0.02f, 0.025f, 1.0f), 0.88f);

        char debugText[256]{};
        if (model.GetChangeRecord().valid)
        {
            const ChangeRecord& change = model.GetChangeRecord();
            std::snprintf(debugText, sizeof(debugText),
                "T:%s  P:%s  C:%zu  H:%d",
                model.GetObjects()[change.objectIndex].name.c_str(),
                GameModel::PropertyName(change.property), model.CountChangedObjects(), hoveredObject);
        }
        else
        {
            std::snprintf(debugText, sizeof(debugText),
                "NO CHANGE  HOVER:%d", hoveredObject);
        }
        DrawText(debugText, Vector2(34.0f, 664.0f), Color(0.56f, 0.84f, 0.92f), 0.46f);
    }

    void GameRenderer::DrawPanel(float x, float y, float width, float height,
        const DirectX::XMFLOAT4& tint, float alpha)
    {
        const Vector2 worldPosition(
            x + width * 0.5f - 640.0f,
            360.0f - (y + height * 0.5f));
        LSpriteDesc2D descriptor(static_cast<UINT>(SpriteId::Solid), worldPosition);
        descriptor.m_fXScale = width / 4.0f;
        descriptor.m_fYScale = height / 4.0f;
        descriptor.m_f4Tint = tint;
        descriptor.m_fAlpha = alpha;
        Draw(&descriptor);
    }

    void GameRenderer::DrawText(const char* text, const Vector2& position,
        DirectX::XMVECTORF32 color, float scale)
    {
        if (m_pFont == nullptr || text == nullptr)
        {
            return;
        }

        wchar_t buffer[512]{};
        MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, _countof(buffer));
        m_pFont->DrawString(m_pSpriteBatch.get(), buffer, position, color,
            0.0f, Vector2::Zero, scale);
    }
}
