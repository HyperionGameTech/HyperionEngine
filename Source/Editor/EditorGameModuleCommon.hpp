/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/StringView.hpp>
#include <Core/Utilities/Time.hpp>

namespace Hyperion {

class EditorProject;
class Game;

class GameModuleError : public Error
{
public:
    GameModuleError()
        : Error()
    {
    }

    template <auto CurrentFunctionString, auto FormatString, class... Args>
    GameModuleError(ValueWrapper<CurrentFunctionString> fnName, ValueWrapper<FormatString> format, Args&&... args)
        : Error(fnName, format, std::forward<Args>(args)...)
    {
    }
};

struct GameModuleBase
{
    FilePath modulePath;
    TResult<Time, GameModuleError> loadedTimestampOrError = Time();

    Time attemptedTimestamp = Time(0);

    // a rebuilt module is only picked up once its timestamp has held for a poll since the build may still be writing it
    Time lastPollTime;
    Time pendingTimestamp;

    bool IsLoaded() const
    {
        return loadedTimestampOrError.HasValue();
    }

    const GameModuleError* GetLoadedError() const
    {
        if (loadedTimestampOrError.HasError())
        {
            return &loadedTimestampOrError.GetError();
        }

        return nullptr;
    }

    Time GetLoadedTimestamp() const
    {
        if (loadedTimestampOrError.HasValue())
        {
            return *loadedTimestampOrError;
        }

        return Time(0);
    }
};

template <class ModuleType>
ModuleType* FindGameModule(
    const Array<UniquePtr<ModuleType>>& modules,
    const FilePath& modulePath)
{
    for (const UniquePtr<ModuleType>& module : modules)
    {
        if (module->modulePath == modulePath)
        {
            return module.Get();
        }
    }

    return nullptr;
}

Mutex& GetGameModulesMutex();

ANSIStringView GetModulePlatformDirectory();

TResult<FilePath> CopyModule(const FilePath& modulePath, ANSIStringView copyDirectoryName);

bool PollModuleRebuilt(GameModuleBase& module);

void CollectManagedGarbage();

void ReplaceProjectGame(EditorProject* project, Handle<Game> oldGame, Handle<Game> newGame);

// EditorManagedModule.cpp
Result ReloadManagedGameModule(EditorProject* project);
void UpdateManagedGameModule(EditorProject* project);
void RetireNativeModule(const FilePath& modulePath);

} // namespace Hyperion
