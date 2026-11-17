#include "GameModel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace SomethingMoved
{
    GameModel::GameModel(std::uint32_t seed)
    {
        if (seed == 0)
        {
            std::random_device device;
            const auto clockSeed = static_cast<std::uint32_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            std::seed_seq sequence{ device(), device(), clockSeed };
            m_random.seed(sequence);
        }
        else
        {
            m_random.seed(seed);
        }

        InitializeObjects();
    }

    void GameModel::InitializeObjects()
    {
        auto addObject = [this](const char* name, const char* changeDescription,
            const char* observationHint, std::size_t spriteIndex, int difficultyTier,
            ChangeProperty property, float x, float y, float hitWidth, float hitHeight)
        {
            RoomObject object;
            object.name = name;
            object.changeDescription = changeDescription;
            object.observationHint = observationHint;
            object.spriteIndex = spriteIndex;
            object.difficultyTier = difficultyTier;
            object.property = property;
            object.baseState.x = x;
            object.baseState.y = y;
            object.currentState = object.baseState;
            object.hitWidth = hitWidth;
            object.hitHeight = hitHeight;
            m_objects.push_back(object);
        };

        // Coordinates are authored directly against the fixed 1280x720
        // Blender render. World origin is the screen center and +Y is up.
        addObject("Teddy bear", "The teddy vanished from the bed.", "Remember the center of the bed.",
            0, 0, ChangeProperty::Visibility, -194.0f, -50.0f, 104.0f, 126.0f);
        addObject("Bedside lamp", "The bedside lamp went dark.", "Watch the warm light beside the bed.",
            1, 1, ChangeProperty::Color, -420.0f, 10.0f, 90.0f, 64.0f);
        addObject("Portrait", "The portrait disappeared.", "Remember what watches from the wall.",
            2, 0, ChangeProperty::Visibility, -346.0f, 118.0f, 108.0f, 132.0f);
        addObject("Wall clock", "The clock hands shifted.", "Check the time above the bed.",
            3, 2, ChangeProperty::Rotation, -31.0f, 154.0f, 86.0f, 86.0f);
        addObject("Telephone", "The telephone disappeared.", "Scan the near edge of the desk.",
            4, 0, ChangeProperty::Visibility, -574.0f, -132.0f, 76.0f, 70.0f);
        addObject("Book row", "The books were taken away.", "Count the objects on the desk.",
            5, 1, ChangeProperty::Visibility, -500.0f, -149.0f, 72.0f, 62.0f);
        addObject("Candle", "The candle flame went out.", "Notice the small flame by the lamp.",
            6, 2, ChangeProperty::Color, -386.0f, -57.0f, 44.0f, 68.0f);
        addObject("Wardrobe", "The wardrobe door opened.", "The wardrobe should stay closed.",
            7, 1, ChangeProperty::Rotation, 395.0f, 74.0f, 224.0f, 286.0f);
    }

    void GameModel::StartNewGame()
    {
        m_round = 1;
        m_score = 0;
        m_mistakes = 0;
        m_lastAnswerCorrect = false;
        m_lastAnswerTimedOut = false;
        m_correctAnswers = 0;
        m_hintsRemaining = HintsPerRound;
        m_dimmedObjects.clear();
        m_lastObjectIndex = static_cast<std::size_t>(-1);
        m_lastProperty = ChangeProperty::None;
        ResetObjects();
        m_phase = GamePhase::Intro;
        m_phaseTimeRemaining = 0.0f;
    }

    void GameModel::BeginTutorial()
    {
        if (m_phase != GamePhase::Intro)
        {
            return;
        }
        m_phase = GamePhase::Observe;
        m_phaseTimeRemaining = ObserveTimeForRound();
    }

    void GameModel::ShowHowTo()
    {
        m_phase = GamePhase::HowTo;
        m_phaseTimeRemaining = 0.0f;
    }

    void GameModel::ReturnToMenu()
    {
        ResetObjects();
        m_phase = GamePhase::Start;
        m_phaseTimeRemaining = 0.0f;
    }

    void GameModel::ResetObjects()
    {
        for (auto& object : m_objects)
        {
            object.currentState = object.baseState;
        }
        m_change = {};
        m_dimmedObjects.clear();
    }

    void GameModel::Update(float deltaSeconds)
    {
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        switch (m_phase)
        {
        case GamePhase::Observe:
            m_phaseTimeRemaining -= deltaSeconds;
            if (m_phaseTimeRemaining <= 0.0f)
            {
                BeginBlackout();
            }
            break;

        case GamePhase::Blackout:
            m_phaseTimeRemaining -= deltaSeconds;
            if (m_phaseTimeRemaining <= 0.0f)
            {
                m_phase = GamePhase::Find;
                m_phaseTimeRemaining = FindTimeForRound();
            }
            break;

        case GamePhase::Find:
            m_phaseTimeRemaining -= deltaSeconds;
            if (m_phaseTimeRemaining <= 0.0f)
            {
                ResolveRound(false, true);
            }
            break;

        case GamePhase::Feedback:
            m_phaseTimeRemaining -= deltaSeconds;
            if (m_phaseTimeRemaining <= 0.0f)
            {
                AdvanceAfterFeedback();
            }
            break;

        default:
            break;
        }
    }

    void GameModel::BeginBlackout()
    {
        GenerateChange();
        m_hintsRemaining = HintsPerRound;
        m_phase = GamePhase::Blackout;
        m_phaseTimeRemaining = BlackoutDuration;
    }

    void GameModel::SubmitSelection(std::size_t objectIndex)
    {
        if (m_phase != GamePhase::Find || objectIndex >= m_objects.size())
        {
            return;
        }

        ResolveRound(m_change.valid && objectIndex == m_change.objectIndex);
    }

    bool GameModel::UseHint()
    {
        if (m_phase != GamePhase::Find || !m_change.valid || m_hintsRemaining <= 0)
        {
            return false;
        }

        // Dim one randomly chosen wrong object: never the changed one, and
        // never one already dimmed by an earlier hint this round.
        std::vector<std::size_t> candidates;
        for (std::size_t index = 0; index < m_objects.size(); ++index)
        {
            if (index == m_change.objectIndex)
            {
                continue;
            }
            if (std::find(m_dimmedObjects.begin(), m_dimmedObjects.end(), index) != m_dimmedObjects.end())
            {
                continue;
            }
            candidates.push_back(index);
        }
        if (candidates.empty())
        {
            return false;
        }

        std::uniform_int_distribution<std::size_t> distribution(0, candidates.size() - 1);
        m_dimmedObjects.push_back(candidates[distribution(m_random)]);
        --m_hintsRemaining;
        m_score = std::max(0, m_score - HintCost);
        return true;
    }

    int GameModel::GetHintsRemaining() const noexcept { return m_hintsRemaining; }
    const std::vector<std::size_t>& GameModel::GetDimmedObjects() const noexcept { return m_dimmedObjects; }

    void GameModel::ResolveRound(bool correct, bool timedOut)
    {
        m_lastAnswerCorrect = correct;
        m_lastAnswerTimedOut = timedOut;
        if (correct)
        {
            ++m_correctAnswers;
            const int timeBonus = static_cast<int>(std::ceil(std::max(0.0f, m_phaseTimeRemaining))) * 2;
            m_score += 100 + (m_round * 10) + timeBonus;
        }
        else
        {
            ++m_mistakes;
        }

        m_phase = GamePhase::Feedback;
        m_phaseTimeRemaining = FeedbackDuration;
    }

    void GameModel::AdvanceAfterFeedback()
    {
        if (m_mistakes >= MaximumMistakes)
        {
            m_phase = GamePhase::GameOver;
            m_phaseTimeRemaining = 0.0f;
            return;
        }

        if (m_round >= TotalRounds)
        {
            m_phase = GamePhase::Victory;
            m_phaseTimeRemaining = 0.0f;
            return;
        }

        ++m_round;
        ResetObjects();
        m_phase = GamePhase::Observe;
        m_phaseTimeRemaining = ObserveTimeForRound();
    }

    void GameModel::GenerateChange()
    {
        ResetObjects();

        std::vector<std::size_t> candidates;
        if (m_round == 1)
        {
            candidates.push_back(0); // A clear, guided first anomaly.
        }
        else if (m_round == TotalRounds)
        {
            candidates.push_back(7); // The wardrobe is the authored finale.
        }
        else
        {
            const int tier = GetDifficultyTier();
            for (std::size_t index = 0; index < m_objects.size(); ++index)
            {
                if (m_objects[index].difficultyTier <= tier && index != m_lastObjectIndex &&
                    !(m_round == TotalRounds - 1 && index == 7))
                {
                    candidates.push_back(index);
                }
            }
        }
        if (candidates.empty())
        {
            for (std::size_t index = 0; index < m_objects.size(); ++index)
            {
                if (index != m_lastObjectIndex) candidates.push_back(index);
            }
        }
        std::uniform_int_distribution<std::size_t> distribution(0, candidates.size() - 1);
        const std::size_t objectIndex = candidates[distribution(m_random)];
        RoomObject& object = m_objects[objectIndex];
        ObjectState modified = object.currentState;
        const ChangeProperty selectedProperty = object.property;

        // Each index maps one-to-one to an authored, aligned full-frame render.
        // The state mutation keeps model validation and scoring independent of
        // the presentation technique while still changing exactly one object.
        switch (objectIndex)
        {
        case 0: // teddy missing
        case 2: // portrait missing
        case 4: // telephone missing
        case 5: // books missing
            modified.visible = false;
            break;
        case 1: // lamp off
        case 6: // candle out
            modified.color.red = 0.30f;
            modified.color.green = 0.30f;
            modified.color.blue = 0.34f;
            break;
        case 3: // clock hands changed
        case 7: // wardrobe opened
            modified.rotation = 0.35f;
            break;
        default:
            modified.visible = false;
            break;
        }

        m_change.valid = true;
        m_change.objectIndex = objectIndex;
        m_change.property = selectedProperty;
        m_change.originalState = object.currentState;
        m_change.modifiedState = modified;
        object.currentState = modified;
        m_lastObjectIndex = objectIndex;
        m_lastProperty = selectedProperty;
    }

    float GameModel::VisualWidth(const RoomObject& object, const ObjectState& state) const noexcept
    {
        return object.hitWidth * state.scaleX / object.baseState.scaleX;
    }

    float GameModel::VisualHeight(const RoomObject& object, const ObjectState& state) const noexcept
    {
        return object.hitHeight * state.scaleY / object.baseState.scaleY;
    }

    int GameModel::HitTest(float worldX, float worldY) const
    {
        for (std::size_t reverseIndex = m_objects.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            const RoomObject& object = m_objects[index];
            const ObjectState& state = object.currentState;
            const float halfWidth = VisualWidth(object, state) * 0.5f;
            const float halfHeight = VisualHeight(object, state) * 0.5f;

            if (worldX >= state.x - halfWidth && worldX <= state.x + halfWidth &&
                worldY >= state.y - halfHeight && worldY <= state.y + halfHeight)
            {
                return static_cast<int>(index);
            }
        }

        return -1;
    }

    void GameModel::DebugAdvancePhase()
    {
        switch (m_phase)
        {
        case GamePhase::Observe:
            BeginBlackout();
            break;
        case GamePhase::Blackout:
            m_phase = GamePhase::Find;
            m_phaseTimeRemaining = FindTimeForRound();
            break;
        case GamePhase::Find:
            ResolveRound(false);
            break;
        case GamePhase::Feedback:
            AdvanceAfterFeedback();
            break;
        default:
            break;
        }
    }

    GamePhase GameModel::GetPhase() const noexcept { return m_phase; }
    float GameModel::GetPhaseTimeRemaining() const noexcept { return std::max(0.0f, m_phaseTimeRemaining); }
    int GameModel::GetRound() const noexcept { return m_round; }
    int GameModel::GetScore() const noexcept { return m_score; }
    int GameModel::GetMistakes() const noexcept { return m_mistakes; }
    bool GameModel::WasLastAnswerCorrect() const noexcept { return m_lastAnswerCorrect; }
    bool GameModel::WasLastAnswerTimedOut() const noexcept { return m_lastAnswerTimedOut; }
    int GameModel::GetCorrectAnswers() const noexcept { return m_correctAnswers; }
    int GameModel::GetDifficultyTier() const noexcept
    {
        if (m_round <= 4) return 0;
        if (m_round <= 7) return 1;
        return 2;
    }
    bool GameModel::IsTutorialRound() const noexcept { return m_round == 1; }
    bool GameModel::IsFinalRound() const noexcept { return m_round == TotalRounds; }
    const std::vector<RoomObject>& GameModel::GetObjects() const noexcept { return m_objects; }
    const ChangeRecord& GameModel::GetChangeRecord() const noexcept { return m_change; }

    float GameModel::GetRoomBrightness() const noexcept
    {
        if (m_phase == GamePhase::Observe)
        {
            const float elapsed = ObserveTimeForRound() - m_phaseTimeRemaining;
            return std::clamp(0.50f + elapsed / 1.4f * 0.50f, 0.50f, 1.0f);
        }
        if (m_phase == GamePhase::Find)
        {
            const float elapsed = FindTimeForRound() - m_phaseTimeRemaining;
            return std::clamp(0.30f + elapsed / 0.75f * 0.70f, 0.30f, 1.0f);
        }
        if (m_phase != GamePhase::Blackout)
        {
            return 1.0f;
        }

        const float elapsed = BlackoutDuration - m_phaseTimeRemaining;
        if (elapsed < 0.70f)
        {
            return (static_cast<int>(elapsed * 18.0f) % 2 == 0) ? 0.72f : 0.08f;
        }
        if (elapsed > 2.35f)
        {
            return (static_cast<int>((elapsed - 2.35f) * 20.0f) % 2 == 0) ? 0.12f : 0.78f;
        }
        return 0.025f;
    }

    float GameModel::ObserveTimeForRound() const noexcept
    {
        if (m_round == 1) return 22.0f;
        if (m_round <= 4) return 18.0f;
        if (m_round <= 7) return 16.0f;
        return 14.0f;
    }

    float GameModel::FindTimeForRound() const noexcept
    {
        if (m_round == 1) return 18.0f;
        if (m_round <= 4) return 15.0f;
        if (m_round <= 7) return 13.0f;
        return 11.0f;
    }

    std::size_t GameModel::CountChangedObjects() const noexcept
    {
        std::size_t count = 0;
        for (const auto& object : m_objects)
        {
            if (StatesDiffer(object.baseState, object.currentState))
            {
                ++count;
            }
        }
        return count;
    }

    bool GameModel::StatesDiffer(const ObjectState& left, const ObjectState& right) noexcept
    {
        constexpr float Epsilon = 0.0001f;
        return std::fabs(left.x - right.x) > Epsilon ||
            std::fabs(left.y - right.y) > Epsilon ||
            std::fabs(left.rotation - right.rotation) > Epsilon ||
            std::fabs(left.scaleX - right.scaleX) > Epsilon ||
            std::fabs(left.scaleY - right.scaleY) > Epsilon ||
            left.visible != right.visible ||
            std::fabs(left.color.red - right.color.red) > Epsilon ||
            std::fabs(left.color.green - right.color.green) > Epsilon ||
            std::fabs(left.color.blue - right.color.blue) > Epsilon;
    }

    const char* GameModel::PhaseName(GamePhase phase) noexcept
    {
        switch (phase)
        {
        case GamePhase::Start: return "START";
        case GamePhase::HowTo: return "HOW TO PLAY";
        case GamePhase::Intro: return "THE FIRST NIGHT";
        case GamePhase::Observe: return "OBSERVE";
        case GamePhase::Blackout: return "BLACKOUT";
        case GamePhase::Find: return "FIND THE CHANGE";
        case GamePhase::Feedback: return "RESULT";
        case GamePhase::Victory: return "VICTORY";
        case GamePhase::GameOver: return "GAME OVER";
        default: return "UNKNOWN";
        }
    }

    const char* GameModel::PropertyName(ChangeProperty property) noexcept
    {
        switch (property)
        {
        case ChangeProperty::Position: return "position";
        case ChangeProperty::Visibility: return "visibility";
        case ChangeProperty::Rotation: return "rotation";
        case ChangeProperty::Scale: return "scale";
        case ChangeProperty::Color: return "color";
        default: return "none";
        }
    }
}
