#pragma once

#include "Component.h"
#include "GameModel.h"
#include "Sound.h"

namespace SomethingMoved
{
    enum class SoundId
    {
        Rain,
        Blackout,
        Correct,
        Incorrect,
        Count
    };

    // Baseline audio facade. Shehtaz can replace this with a full AudioManager
    // without changing GameApp's phase-event integration points.
    class AudioController final : private LComponent
    {
    public:
        bool Initialize();
        void BeginFrame();
        void Update(GamePhase phase, bool answerCorrect, bool mutePressed);
        void Shutdown();

        [[nodiscard]] bool IsMuted() const noexcept { return m_muted; }
        [[nodiscard]] bool IsAvailable() const noexcept { return m_initialized; }

    private:
        void StartRain();

        bool m_initialized = false;
        bool m_muted = false;
        GamePhase m_previousPhase = GamePhase::Start;
        LSoundDesc m_rainLoop;
    };
}
