/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONSOLE_COMMAND_MANAGER_HPP
#define HYPERION_CONSOLE_COMMAND_MANAGER_HPP

#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/Result.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class ConsoleCommandBase;
class ConsoleCommandManagerImpl;

HYP_CLASS()

class HYP_API ConsoleCommandManager
{
    HYP_OBJECT_BODY(ConsoleCommandManager);

public:
    static ConsoleCommandManager& GetInstance();

    ConsoleCommandManager();
    ConsoleCommandManager(const ConsoleCommandManager& other) = delete;
    ConsoleCommandManager& operator=(const ConsoleCommandManager& other) = delete;
    ConsoleCommandManager(ConsoleCommandManager&& other) noexcept = default;
    ConsoleCommandManager& operator=(ConsoleCommandManager&& other) noexcept = default;
    virtual ~ConsoleCommandManager();

    void Initialize();
    void Shutdown();

    void RegisterCommand(const RC<ConsoleCommandBase>& command);

    Result ExecuteCommand(const String& command_line);

private:
    int FindAndRegisterCommands();

    UniquePtr<ConsoleCommandManagerImpl> m_impl;
};

} // namespace hyperion

#endif
