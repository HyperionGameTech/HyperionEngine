#ifndef HYPERION_SYSTEM_COMMAND_QUEUE_HPP
#define HYPERION_SYSTEM_COMMAND_QUEUE_HPP

#include <core/Containers.hpp>
#include <core/system/SharedMemory.hpp>
#include <Types.hpp>

namespace hyperion {

enum class CommandName : UInt
{

};

struct CommandEntry
{
    CommandName command_name;
    UByte flags;
    DynArray<String> args;
};

class CommandQueue
{
public:
    static constexpr UInt8 magic = 0xAE;

    bool IsLocked() const
        { return m_flags & 0x1; }

    DynArray<CommandEntry> &GetEntries()
        { return m_entries; }

    const DynArray<CommandEntry> &GetEntries() const
        { return m_entries; }

    static bool ReadCommandQueue(const UByte *data, SizeType size, CommandQueue &out)
    {
        UInt index = 0u;

        if (size < 6) { // one byte for magic, one byte for flags, 4 for size are minimum.
            return false;
        }

        if (data[index++] != magic) {
            return false;
        }

        out.m_flags = data[index++];

        // next 4 bytes = size of list in COMMANDS
        union {
            UByte size_bytes[4];
            UInt32 size_uint;
        };


        for (UInt i = 0; i < 4; i++) {
            if (index >= size) {
                return false;
            }

            size_bytes[i] = data[index++];
        }
        
        if (size_uint == 0) {
            return true; // was valid, but no items exist
        }

        DebugLog(LogType::Debug, "size_uint = %u\n", size_uint);

        while (size_uint) {
            CommandEntry command;

            // read each item
            // a command consists of UInt32 = command enum
            // uint32 for number of args
            // and a variable number of serialized string arguments,
            // which each with a UInt32 length param

            // read enum member
            union {
                UByte command_name_bytes[4];
                UInt32 command_name_uint;
            };

            for (UInt i = 0; i < 4; i++) {
                if (index >= size) {
                    return false;
                }

                command_name_bytes[i] = data[index++];
            }

            command.command_name = static_cast<CommandName>(command_name_uint);

            // read command flags as byte
            if (index >= size) {
                return false;
            }

            command.flags = data[index++];

            // now we read number of args
            union {
                UByte num_args_bytes[4];
                UInt32 num_args_uint;
            };

            for (UInt i = 0; i < 4; i++) {
                if (index >= size) {
                    return false;
                }

                num_args_bytes[i] = data[index++];
            }

            command.args.Reserve(num_args_uint);

            while (num_args_uint) {
                // read uint32 of string length.
                union {
                    UByte string_length_bytes[4];
                    UInt32 string_length_uint;
                };

                for (UInt i = 0; i < 4; i++) {
                    if (index >= size) {
                        return false;
                    }

                    string_length_bytes[i] = data[index++];
                }

                if (string_length_uint != 0) {
                    char *str = new char[string_length_uint + 1];

                    for (UInt i = 0; i < string_length_uint; i++) {
                        if (index >= size) {
                            delete[] str;
                            return false;
                        }

                        str[i] = data[index++];
                    }

                    str[string_length_uint] = '\0';

                    command.args.PushBack(str);

                    delete[] str;
                } else {
                    command.args.PushBack(String::empty);
                }

                --num_args_uint;
            }

            out.m_entries.PushBack(std::move(command));

            --size_uint;
        }

        return true;
    }

    DynArray<UByte> Serialize() const
    {
        DynArray<UByte> out;
        out.PushBack(magic);
        out.PushBack(m_flags);
    
        out.PushBack((static_cast<UInt32>(m_entries.Size()) & 0xff000000) >> 24);
        out.PushBack((static_cast<UInt32>(m_entries.Size()) & 0x00ff0000) >> 16);
        out.PushBack((static_cast<UInt32>(m_entries.Size()) & 0x0000ff00) >> 8);
        out.PushBack((static_cast<UInt32>(m_entries.Size()) & 0x000000ff));

        for (auto &command : m_entries) {
            // command name (enum / uint32)
            out.PushBack((static_cast<UInt32>(command.command_name) & 0xff000000) >> 24);
            out.PushBack((static_cast<UInt32>(command.command_name) & 0x00ff0000) >> 16);
            out.PushBack((static_cast<UInt32>(command.command_name) & 0x0000ff00) >> 8);
            out.PushBack((static_cast<UInt32>(command.command_name) & 0x000000ff));

            // command flags (byte)
            out.PushBack(command.flags);

            // command # of args (uint32)
            out.PushBack((static_cast<UInt32>(command.args.Size()) & 0xff000000) >> 24);
            out.PushBack((static_cast<UInt32>(command.args.Size()) & 0x00ff0000) >> 16);
            out.PushBack((static_cast<UInt32>(command.args.Size()) & 0x0000ff00) >> 8);
            out.PushBack((static_cast<UInt32>(command.args.Size()) & 0x000000ff));

            for (auto &argument : command.args) {
                // string size (uint32) note we encode size not length, as we are counting in bytes
                out.PushBack((static_cast<UInt32>(argument.Size()) & 0xff000000) >> 24);
                out.PushBack((static_cast<UInt32>(argument.Size()) & 0x00ff0000) >> 16);
                out.PushBack((static_cast<UInt32>(argument.Size()) & 0x0000ff00) >> 8);
                out.PushBack((static_cast<UInt32>(argument.Size()) & 0x000000ff));

                for (SizeType i = 0; i < argument.Size(); i++) {
                    out.PushBack(static_cast<UByte>(argument.GetRawChar(i)));
                }
            }
        }

        return out;
    }

    void Write(SharedMemory &shared_memory)
    {
        const auto bytes = Serialize();

        shared_memory.Write(&bytes[0], bytes.Size());
    }

private:
    UByte m_flags;
    DynArray<CommandEntry> m_entries;
};

} // namespace hyperion

#endif