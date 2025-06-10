/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/ConsoleCommandManager.hpp>
#include <console/ConsoleCommand.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DEFINE_LOG_SUBCHANNEL(Console, Core);

#pragma region ConsoleCommandManagerImpl

static ANSIStringView ConsoleCommand_KeyByFunction(const Handle<ConsoleCommandBase>& command)
{
    return command->InstanceClass()->GetAttribute("command").GetString().Data();
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
{
    m_impl = MakeUnique<ConsoleCommandManagerImpl>();
}

ConsoleCommandManager::~ConsoleCommandManager()
{
}

void ConsoleCommandManager::Initialize()
{
    int num_registered_commands = FindAndRegisterCommands();

    if (num_registered_commands > 0)
    {
        HYP_LOG(Console, Info, "Registered {} console command(s)", num_registered_commands);
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
    const HypClass* parent_hyp_class = ConsoleCommandBase::Class();

    Array<Handle<ConsoleCommandBase>> commands;

    HypClassRegistry::GetInstance().ForEachClass([this, parent_hyp_class, &commands](const HypClass* hyp_class)
        {
            if (hyp_class->IsAbstract())
            {
                return IterationResult::CONTINUE;
            }

            if (hyp_class->HasParent(parent_hyp_class))
            {
                HypData hyp_data;
                if (!hyp_class->CreateInstance(hyp_data))
                {
                    HYP_LOG(Console, Error, "Failed to create instance of class: {}", hyp_class->GetName());

                    return IterationResult::CONTINUE;
                }

                commands.PushBack(std::move(hyp_data.Get<Handle<ConsoleCommandBase>>()));

                return IterationResult::CONTINUE;
            }

            return IterationResult::CONTINUE;
        });

    if (commands.Empty())
    {
        return 0;
    }

    Mutex::Guard guard(m_impl->m_mutex);

    int num_registered_commands = 0;

    for (const Handle<ConsoleCommandBase>& command : commands)
    {
        if (!command->InstanceClass()->GetAttribute("command"))
        {
            HYP_LOG(Console, Error, "Command must have a `command` attribute");

            continue;
        }

        command->m_definitions = command->GetDefinitions_Internal();

        HYP_LOG(Console, Info, "Registering command: {}\tClass: {}",
            command->InstanceClass()->GetAttribute("command").GetString(),
            command->InstanceClass()->GetName());

        m_impl->m_commands.Set(std::move(command));

        ++num_registered_commands;
    }

    return num_registered_commands;
}

void ConsoleCommandManager::RegisterCommand(const Handle<ConsoleCommandBase>& command)
{
    if (!command)
    {
        return;
    }

    if (!command->InstanceClass()->GetAttribute("command"))
    {
        HYP_LOG(Console, Error, "Command must have a `command` attribute");

        return;
    }

    Mutex::Guard guard(m_impl->m_mutex);
    command->m_definitions = command->GetDefinitions_Internal();
    m_impl->m_commands.Set(command);
}

Result ConsoleCommandManager::ExecuteCommand(const String& command_line)
{
    if (command_line.Empty())
    {
        return {};
    }

    Array<String> split = command_line.Trimmed().Split(' ');

    if (split.Empty())
    {
        return {};
    }

    String command_name = split[0].ToLower();

    Mutex::Guard guard(m_impl->m_mutex);

    auto it = m_impl->m_commands.Find(command_name.Data());

    if (it == m_impl->m_commands.End())
    {
        HYP_LOG(Console, Error, "Command not found: {}", command_name);

        return HYP_MAKE_ERROR(Error, "Command not found: {}", command_name);
    }

    const CommandLineArgumentDefinitions& definitions = (*it)->GetDefinitions();

    CommandLineParser command_line_parser { &definitions };

    if (auto parse_result = command_line_parser.Parse(command_line); parse_result.HasValue())
    {
        return (*it)->Execute(parse_result.GetValue());
    }
    else
    {
        HYP_LOG(Console, Error, "Failed to parse command line: {}", parse_result.GetError().GetMessage());

        return parse_result.GetError();
    }

    return {};
}

#pragma endregion ConsoleCommandManager

} // namespace hyperion
