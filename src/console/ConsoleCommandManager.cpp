/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/ConsoleCommandManager.hpp>
#include <console/ConsoleCommand.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>

#include <core/reflection/HypClass.hpp>
#include <core/reflection/HypClassRegistry.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

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
        HYP_LOG(Console, Info, "Registered {} console command(s)", numRegisteredCommands);
    }
    else
    {
        HYP_LOG(Console, Info, "No console commands registered");
    }
}

void ConsoleCommandManager::Shutdown()
{
    Mutex::Guard guard(m_impl->m_mutex);

    m_impl->m_commands.Clear();
}

int ConsoleCommandManager::FindAndRegisterCommands()
{
    const HypClass* parentHypClass = ConsoleCommandBase::Class();

    Array<Handle<ConsoleCommandBase>> commands;

    HypClassRegistry::GetInstance().ForEachClass([this, parentHypClass, &commands](const HypClass* hypClass)
        {
            if (hypClass->IsDerivedFrom(parentHypClass))
            {
                if (hypClass->IsAbstract())
                {
                    HYP_LOG(Console, Error, "Class '{}' is abstract, cannot register console command", hypClass->GetName());

                    return IterationResult::CONTINUE;
                }

                HypData hypData;
                if (!hypClass->CreateInstance(hypData))
                {
                    HYP_LOG(Console, Error, "Failed to create instance of class: {}", hypClass->GetName());

                    return IterationResult::CONTINUE;
                }

                commands.PushBack(std::move(hypData.Get<Handle<ConsoleCommandBase>>()));

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

        HYP_LOG(Console, Info, "Registering command: {}\tClass: {}",
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

    Mutex::Guard guard(m_impl->m_mutex);

    auto it = m_impl->m_commands.Find(commandName.Data());

    if (it == m_impl->m_commands.End())
    {
        HYP_LOG(Console, Error, "Command not found: {}", commandName);

        return HYP_MAKE_ERROR(Error, "Command not found: {}", commandName);
    }

    const CommandLineArgumentDefinitions& definitions = (*it)->GetDefinitions();

    CommandLineParser commandLineParser { &definitions };

    if (auto parseResult = commandLineParser.Parse(commandLine); parseResult.HasValue())
    {
        return (*it)->Execute(parseResult.GetValue());
    }
    else
    {
        HYP_LOG(Console, Error, "Failed to parse command line: {}", parseResult.GetError().GetMessage());

        return parseResult.GetError();
    }

    return {};
}

#pragma endregion ConsoleCommandManager

} // namespace hyperion
