#include "AudioController.h"

#include <filesystem>

namespace SomethingMoved
{
    bool AudioController::Initialize()
    {
        const std::filesystem::path audioRoot = L"Media\\Audio";
        if (!std::filesystem::exists(audioRoot / L"rain.wav") ||
            !std::filesystem::exists(audioRoot / L"blackout.wav") ||
            !std::filesystem::exists(audioRoot / L"correct.wav") ||
            !std::filesystem::exists(audioRoot / L"incorrect.wav"))
        {
            return false;
        }

        try
        {
            m_pAudio->Initialize(SoundId::Count);
            m_pAudio->Load(SoundId::Rain, "rain");
            m_pAudio->Load(SoundId::Blackout, "blackout");
            m_pAudio->Load(SoundId::Correct, "correct");
            m_pAudio->Load(SoundId::Incorrect, "incorrect");
            m_initialized = true;
            StartRain();
        }
        catch (...)
        {
            m_initialized = false;
        }
        return m_initialized;
    }

    void AudioController::BeginFrame()
    {
        if (m_initialized)
        {
            m_pAudio->BeginFrame();
        }
    }

    void AudioController::Update(GamePhase phase, bool answerCorrect, bool mutePressed)
    {
        if (!m_initialized)
        {
            return;
        }

        if (mutePressed)
        {
            m_pAudio->mute();
            m_muted = !m_muted;
        }

        if (phase != m_previousPhase)
        {
            if (phase == GamePhase::Blackout)
            {
                m_pAudio->play(SoundId::Blackout, 0.72f);
            }
            else if (phase == GamePhase::Feedback)
            {
                m_pAudio->play(answerCorrect ? SoundId::Correct : SoundId::Incorrect, 0.82f);
            }
            else if (phase == GamePhase::Victory)
            {
                m_pAudio->play(SoundId::Correct, 0.68f);
            }
            else if (phase == GamePhase::GameOver)
            {
                m_pAudio->play(SoundId::Incorrect, 0.68f);
            }
            m_previousPhase = phase;
        }
    }

    void AudioController::Shutdown()
    {
        if (m_initialized)
        {
            m_pAudio->stop();
            m_initialized = false;
        }
    }

    void AudioController::StartRain()
    {
        if (m_initialized && !m_muted)
        {
            m_rainLoop = m_pAudio->loop(SoundId::Rain);
        }
    }
}
