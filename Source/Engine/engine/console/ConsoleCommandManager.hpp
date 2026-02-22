/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/reflection/Handle.hpp>

namespace Hyperion {

class ConsoleCommandBase;
class ConsoleCommandManagerImpl;

class HYP_API ConsoleCommandManager
{
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

    void RegisterCommand(const Handle<ConsoleCommandBase>& command);

    Result ExecuteCommand(const String& commandLine);

private:
    int FindAndRegisterCommands();

    Pimpl<ConsoleCommandManagerImpl> m_impl;
};

} // namespace Hyperion
