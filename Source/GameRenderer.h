#pragma once

#include "GameModel.h"
#include "SpriteRenderer.h"

#include <array>

namespace SomethingMoved
{
    enum class SpriteId
    {
        RoomNormal,
        RoomTeddyMissing,
        RoomLampOff,
        RoomPortraitMissing,
        RoomClockChanged,
        RoomPhoneMissing,
        RoomBooksMissing,
        RoomCandleOut,
        RoomWardrobeOpen,
        Solid,
        Glow,
        Count
    };

    class GameRenderer : public LSpriteRenderer
    {
    public:
        GameRenderer();
        void InitializeGameRenderer();
        void Render(const GameModel& model, int hoveredObject, bool showDebug,
            int menuSelection, bool muted, bool paused, float visualTime);

    private:
        void DrawRoom(const GameModel& model, int hoveredObject);
        void DrawAtmosphere(const GameModel& model);
        void DrawRain();
        void DrawVignette();
        void DrawHud(const GameModel& model, bool muted);
        void DrawOverlay(const GameModel& model, int menuSelection, bool muted, bool paused);
        void DrawDebug(const GameModel& model, int hoveredObject);
        void DrawPanel(float x, float y, float width, float height,
            const DirectX::XMFLOAT4& tint, float alpha);
        void DrawText(const char* text, const Vector2& position,
            DirectX::XMVECTORF32 color, float scale = 1.0f);

        float m_visualTime = 0.0f;
        float m_lastVisualTime = 0.0f;
        std::array<float, 12> m_hoverAlpha{};
    };
}
