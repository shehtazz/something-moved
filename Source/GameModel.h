#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace SomethingMoved
{
    enum class GamePhase
    {
        Start,
        HowTo,
        Intro,
        Observe,
        Blackout,
        Find,
        Feedback,
        Victory,
        GameOver
    };

    enum class ChangeProperty
    {
        None,
        Position,
        Visibility,
        Rotation,
        Scale,
        Color
    };

    struct ColorState
    {
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        float alpha = 1.0f;
    };

    struct ObjectState
    {
        float x = 0.0f;
        float y = 0.0f;
        float rotation = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        bool visible = true;
        ColorState color;
    };

    struct RoomObject
    {
        std::string name;
        std::string changeDescription;
        std::string observationHint;
        std::size_t spriteIndex = 0;
        ObjectState baseState;
        ObjectState currentState;
        float hitWidth = 100.0f;
        float hitHeight = 100.0f;
        int difficultyTier = 0;
        ChangeProperty property = ChangeProperty::None;
    };

    struct ChangeRecord
    {
        bool valid = false;
        std::size_t objectIndex = 0;
        ChangeProperty property = ChangeProperty::None;
        ObjectState originalState;
        ObjectState modifiedState;
    };

    class GameModel
    {
    public:
        static constexpr int TotalRounds = 10;
        static constexpr int MaximumMistakes = 3;
        static constexpr float ObserveDuration = 22.0f;
        static constexpr float BlackoutDuration = 3.0f;
        static constexpr float FindDuration = 18.0f;
        static constexpr float FeedbackDuration = 2.0f;
        static constexpr int HintsPerRound = 2;
        static constexpr int HintCost = 100;

        explicit GameModel(std::uint32_t seed = 0);

        void StartNewGame();
        void BeginTutorial();
        void ShowHowTo();
        void ReturnToMenu();
        void Update(float deltaSeconds);
        void SubmitSelection(std::size_t objectIndex);
        bool UseHint();
        void DebugAdvancePhase();

        [[nodiscard]] int HitTest(float worldX, float worldY) const;
        [[nodiscard]] GamePhase GetPhase() const noexcept;
        [[nodiscard]] float GetPhaseTimeRemaining() const noexcept;
        [[nodiscard]] int GetRound() const noexcept;
        [[nodiscard]] int GetScore() const noexcept;
        [[nodiscard]] int GetMistakes() const noexcept;
        [[nodiscard]] bool WasLastAnswerCorrect() const noexcept;
        [[nodiscard]] bool WasLastAnswerTimedOut() const noexcept;
        [[nodiscard]] int GetCorrectAnswers() const noexcept;
        [[nodiscard]] int GetDifficultyTier() const noexcept;
        [[nodiscard]] bool IsTutorialRound() const noexcept;
        [[nodiscard]] bool IsFinalRound() const noexcept;
        [[nodiscard]] float GetRoomBrightness() const noexcept;
        [[nodiscard]] const std::vector<RoomObject>& GetObjects() const noexcept;
        [[nodiscard]] const ChangeRecord& GetChangeRecord() const noexcept;
        [[nodiscard]] std::size_t CountChangedObjects() const noexcept;
        [[nodiscard]] int GetHintsRemaining() const noexcept;
        [[nodiscard]] const std::vector<std::size_t>& GetDimmedObjects() const noexcept;

        static const char* PhaseName(GamePhase phase) noexcept;
        static const char* PropertyName(ChangeProperty property) noexcept;

    private:
        void InitializeObjects();
        void ResetObjects();
        void BeginBlackout();
        void ResolveRound(bool correct, bool timedOut = false);
        void AdvanceAfterFeedback();
        void GenerateChange();
        float VisualWidth(const RoomObject& object, const ObjectState& state) const noexcept;
        float VisualHeight(const RoomObject& object, const ObjectState& state) const noexcept;
        float ObserveTimeForRound() const noexcept;
        float FindTimeForRound() const noexcept;
        static bool StatesDiffer(const ObjectState& left, const ObjectState& right) noexcept;

        GamePhase m_phase = GamePhase::Start;
        float m_phaseTimeRemaining = 0.0f;
        int m_round = 1;
        int m_score = 0;
        int m_mistakes = 0;
        bool m_lastAnswerCorrect = false;
        bool m_lastAnswerTimedOut = false;
        int m_correctAnswers = 0;
        std::vector<RoomObject> m_objects;
        ChangeRecord m_change;
        std::mt19937 m_random;
        std::size_t m_lastObjectIndex = static_cast<std::size_t>(-1);
        ChangeProperty m_lastProperty = ChangeProperty::None;
        int m_hintsRemaining = HintsPerRound;
        std::vector<std::size_t> m_dimmedObjects;
    };
}
