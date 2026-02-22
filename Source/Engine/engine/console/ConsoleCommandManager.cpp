/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/console/ConsoleCommandManager.hpp>
#include <engine/console/ConsoleCommand.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Thread.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/utilities/StringView.hpp>

#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DEFINE_LOG_SUBCHANNEL(Console, Core);

#pragma region ConsoleCommandManagerImpl

static ANSIStringView ConsoleCommand_KeyByFunction(const Handle<ConsoleCommandBase>& command)
{
    return command->InstanceClass()->GetAttribute(Attributes::g_attrCommand).GetString().Data();
}

class ConsoleCommandManagerImpl
{
public:
    ConsoleCommandManagerImpl()
    {
    }

    ~ConsoleCommandManagerImpl()
    {
    }

    Mutex m_mutex;
    HashSet<Handle<ConsoleCommandBase>, &ConsoleCommand_KeyByFunction> m_commands;
};

#pragma endregion ConsoleCommandManagerImpl

#pragma region ConsoleCommandManager

ConsoleCommandManager& ConsoleCommandManager::GetInstance()
{
    static ConsoleCommandManager instance;

    return instance;
}

ConsoleCommandManager::ConsoleCommandManager()
    : m_impl(MakePimpl<ConsoleCommandManagerImpl>())
{
}

ConsoleCommandManager::~ConsoleCommandManager()
{
}

void ConsoleCommandManager::Initialize()
{
    int numRegisteredCommands = FindAndRegisterCommands();

    if (numRegisteredCommands > 0)
    {
        HYP_LOG(Console, Verbose, "Registered {} console command(s)", numRegisteredCommands);
    }
    else
    {
        HYP_LOG(Console, Verbose, "No console commands registered");
    }
}

void ConsoleCommandManager::Shutdown()
{
    Mutex::Guard guard(m_impl->m_mutex);

    m_impl->m_commands.Clear();
}

int ConsoleCommandManager::FindAndRegisterCommands()
{
    const Class* parentClass = ConsoleCommandBase::StaticClass();

    Array<Handle<ConsoleCommandBase>> commands;

    ClassRegistry::GetInstance().ForEachClass([this, parentClass, &commands](const Class* cls)
        {
            if (cls->IsDerivedFrom(parentClass) && cls != ConsoleCommandBase::StaticClass())
            {
                if (cls->IsAbstract())
                {
                    HYP_LOG(Console, Verbose, "Class '{}' is abstract, skipping console command registration", cls->GetName());

                    return IterationResult::CONTINUE;
                }

                BoxedValue boxed;
                if (!cls->CreateInstance(boxed))
                {
                    HYP_LOG(Console, Error, "Failed to create instance of class: {}", cls->GetName());

                    return IterationResult::CONTINUE;
                }

                commands.PushBack(std::move(boxed.Get<Handle<ConsoleCommandBase>>()));

                return IterationResult::CONTINUE;
            }

            return IterationResult::CONTINUE;
        });

    if (commands.Empty())
    {
        return 0;
    }

    Mutex::Guard guard(m_impl->m_mutex);

    int numRegisteredCommands = 0;

    for (const Handle<ConsoleCommandBase>& command : commands)
    {
        if (!command->InstanceClass()->GetAttribute(Attributes::g_attrCommand))
        {
            HYP_LOG(Console, Error, "Command must have a `command` attribute");

            continue;
        }

        command->m_definitions = command->GetDefinitions_Internal();

        HYP_LOG(Console, Verbose, "Registering command: {}\tClass: {}",
            command->InstanceClass()->GetAttribute(Attributes::g_attrCommand).GetString(),
            command->InstanceClass()->GetName());

        m_impl->m_commands.Set(std::move(command));

        ++numRegisteredCommands;
    }

    return numRegisteredCommands;
}

void ConsoleCommandManager::RegisterCommand(const Handle<ConsoleCommandBase>& command)
{
    if (!command)
    {
        return;
    }

    if (!command->InstanceClass()->GetAttribute(Attributes::g_attrCommand))
    {
        HYP_LOG(Console, Error, "Command must have a `command` attribute");

        return;
    }

    Mutex::Guard guard(m_impl->m_mutex);
    command->m_definitions = command->GetDefinitions_Internal();
    m_impl->m_commands.Set(command);
}

Result ConsoleCommandManager::ExecuteCommand(const String& commandLine)
{
    if (commandLine.Empty())
    {
        return {};
    }

    Array<String> split = commandLine.Trimmed().Split(' ');

    if (split.Empty())
    {
        return {};
    }

    String commandName = split[0].ToLower();

    Task<Result> task;

    {

        Mutex::Guard guard(m_impl->m_mutex);

        auto it = m_impl->m_commands.Find(commandName.Data());

        auto& lcConsole = g_logChannel_Console;

        if (it == m_impl->m_commands.End())
        {
            HYP_LOG(Console, Error, "Command not found: {}", commandName);

            return HYP_MAKE_ERROR(Error, "Command not found: {}", commandName);
        }

        const CommandLineArgumentDefinitions& definitions = (*it)->GetDefinitions();

        CommandLineParser commandLineParser { &definitions };

        if (auto parseResult = commandLineParser.Parse(commandLine); parseResult.HasValue())
        {
            // execute all commands on the sim thread
            if (IsOnThread(g_simThread))
            {
                return (*it)->Execute(parseResult.GetValue());
            }
            else
            {
                task = GetThreadById(g_simThread)->GetScheduler().Enqueue([command = *it, parseResult = std::move(parseResult)]() mutable
                    {
                        return command->Execute(parseResult.GetValue());
                    });
            }
        }
        else
        {
            HYP_LOG(Console, Error, "Failed to parse command line: {}", parseResult.GetError().GetMessage());

            return parseResult.GetError();
        }
    }

    if (task.IsValid())
    {
        return task.Await();
    }

    return {};
}

#pragma endregion ConsoleCommandManager

} // namespace Hyperion
