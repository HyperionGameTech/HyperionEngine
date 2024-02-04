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
    UByte           flags;
    Array<String>   args;
};

class CommandQueue
{
public:
    static constexpr UInt8 magic = 0xAE;

    bool IsLocked() const
        { return m_flags & 0x1; }

    Array<CommandEntry> &GetEntries()
        { return m_entries; }

    const Array<CommandEntry> &GetEntries() const
        { return m_entries; }

    /*! \brief Reads a command queue from the given data.
     *
     *  \param data The data to read the command queue from.
     *  \param size The size of the data.
     *  \param out The command queue to write the read command queue to.
     *
     *  \return The number of commands read from the data.
     */
    static UInt32 ReadCommandQueue(const UByte *data, SizeType size, CommandQueue &out)
    {
        UInt index = 0u;

        if (size < 6) { // one byte for magic, one byte for flags, 4 for size are minimum.
            return 0;
        }

        if (data[index++] != magic) {
            return 0;
        }

        out.m_flags = data[index++];

        // next 4 bytes = size of list in COMMANDS
        UInt32 size_uint = 0;
        size_uint |= (UInt32(data[index++]) << 24);
        size_uint |= (UInt32(data[index++]) << 16);
        size_uint |= (UInt32(data[index++]) << 8);
        size_uint |= (UInt32(data[index++]));
        
        if (size_uint == 0) {
            return 0;
        }

        const UInt32 original_size = size_uint;

        DebugLog(LogType::Debug, "size_uint = %u\n", size_uint);

        while (size_uint) {
            CommandEntry command;

            // read each item
            // a command consists of UInt32 = command enum
            // uint32 for number of args
            // and a variable number of serialized string arguments,
            // which each with a UInt32 length param

            // read enum member
            UInt32 command_name_uint = 0;
            command_name_uint |= (UInt32(data[index++]) << 24);
            command_name_uint |= (UInt32(data[index++]) << 16);
            command_name_uint |= (UInt32(data[index++]) << 8);
            command_name_uint |= (UInt32(data[index++]));

            command.command_name = static_cast<CommandName>(command_name_uint);

            // read command flags as byte
            if (index >= size) {
                // invalid data
                return 0;
            }

            command.flags = data[index++];

            // now we read number of args
            UInt32 num_args_uint = 0;
            num_args_uint |= (UInt32(data[index++]) << 24);
            num_args_uint |= (UInt32(data[index++]) << 16);
            num_args_uint |= (UInt32(data[index++]) << 8);
            num_args_uint |= (UInt32(data[index++]));

            command.args.Reserve(num_args_uint);

            while (num_args_uint) {
                // read uint32 of string length.
                UInt32 string_length_uint = 0;
                string_length_uint |= (UInt32(data[index++]) << 24);
                string_length_uint |= (UInt32(data[index++]) << 16);
                string_length_uint |= (UInt32(data[index++]) << 8);
                string_length_uint |= (UInt32(data[index++]));

                if (string_length_uint != 0) {
                    char *str = new char[string_length_uint + 1];

                    for (UInt i = 0; i < string_length_uint; i++) {
                        if (index >= size) {
                            // invalid data
                            delete[] str;
                            return 0;
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

        return original_size;
    }

    Array<UByte> Serialize() const
    {
        Array<UByte> out;
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
                    out.PushBack(static_cast<UByte>(argument[i]));
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
    Array<CommandEntry> m_entries;
};

} // namespace hyperion

#endif