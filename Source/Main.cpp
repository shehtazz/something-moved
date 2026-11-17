#include <Windows.h>

#include "GameApp.h"

#include <cstring>
#include <filesystem>

namespace
{
    bool ConfigureMediaWorkingDirectory()
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        {
            return false;
        }

        std::filesystem::path candidate = std::filesystem::path(modulePath).parent_path();
        for (int level = 0; level < 5 && !candidate.empty(); ++level)
        {
            const auto settingsPath = candidate / L"Media" / L"xml" / L"gamesettings.xml";
            if (std::filesystem::exists(settingsPath))
            {
                return SetCurrentDirectoryW(candidate.c_str()) != FALSE;
            }
            candidate = candidate.parent_path();
        }

        return false;
    }
}

int WINAPI WinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE previousInstance,
    _In_ LPSTR commandLine,
    _In_ int showCommand)
{
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(showCommand);

    ConfigureMediaWorkingDirectory();

    if (commandLine != nullptr && std::strstr(commandLine, "--self-test") != nullptr)
    {
        return SomethingMoved::RunModelSelfTests() ? 0 : 2;
    }

    SomethingMoved::GameApp game;
    return game.Run(instance);
}
