/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SYSTEM_COMMAND_QUEUE_HPP
#define HYPERION_SYSTEM_COMMAND_QUEUE_HPP

#include <core/Containers.hpp>
#include <core/system/SharedMemory.hpp>

#include <Types.hpp>

namespace hyperion {

enum class CommandName : uint
{

};

struct CommandEntry
{
    CommandName     command_name;
    ubyte           flags;
    String          json_string;
};

class CommandQueue
{
public:
    static constexpr uint8 magic = 0xAE;
    static constexpr SizeType max_size = 1024 * 1024;

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
    static uint32 ReadCommandQueue(const ubyte *data, SizeType size, CommandQueue &out)
    {
        uint index = 0u;

        if (size < 6) { // one byte for magic, one byte for flags, 4 for size are minimum.
            return 0;
        }

        if (data[index++] != magic) {
            return 0;
        }

        out.m_flags = data[index++];

        // next 4 bytes = size of list in COMMANDS
        uint32 size_uint = 0;
        size_uint |= (uint32(data[index++]) << 24);
        size_uint |= (uint32(data[index++]) << 16);
        size_uint |= (uint32(data[index++]) << 8);
        size_uint |= (uint32(data[index++]));
        
        if (size_uint == 0) {
            return 0;
        }

        if (size_uint > max_size) {
            return 0;
        }

        const uint32 original_size = size_uint;

        DebugLog(LogType::Debug, "size_uint = %u\n", size_uint);

        while (size_uint) {
            CommandEntry command;

            // read each item
            // a command consists of uint32 = command enum
            // uint32 = length of json string
            // json string

            // read enum member
            uint32 command_name_uint = 0;
            command_name_uint |= (uint32(data[index++]) << 24);
            command_name_uint |= (uint32(data[index++]) << 16);
            command_name_uint |= (uint32(data[index++]) << 8);
            command_name_uint |= (uint32(data[index++]));

            command.command_name = static_cast<CommandName>(command_name_uint);

            // read command flags as byte
            if (index >= size) {
                // invalid data
                return 0;
            }

            command.flags = data[index++];

            // read uint32 of string length.
            uint32 string_length_uint = 0;
            string_length_uint |= (uint32(data[index++]) << 24);
            string_length_uint |= (uint32(data[index++]) << 16);
            string_length_uint |= (uint32(data[index++]) << 8);
            string_length_uint |= (uint32(data[index++]));

            if (string_length_uint != 0) {
                char *str = new char[string_length_uint + 1];

                for (uint i = 0; i < string_length_uint; i++) {
                    if (index >= size) {
                        // invalid data
                        delete[] str;
                        return 0;
                    }

                    str[i] = data[index++];
                }

                str[string_length_uint] = '\0';

                command.json_string = String(str);

                delete[] str;
            }

            out.m_entries.PushBack(std::move(command));

            --size_uint;
        }

        return original_size;
    }

    Array<ubyte> Serialize() const
    {
        Array<ubyte> out;
        out.PushBack(magic);
        out.PushBack(m_flags);
    
        out.PushBack((static_cast<uint32>(m_entries.Size()) & 0xff000000) >> 24);
        out.PushBack((static_cast<uint32>(m_entries.Size()) & 0x00ff0000) >> 16);
        out.PushBack((static_cast<uint32>(m_entries.Size()) & 0x0000ff00) >> 8);
        out.PushBack((static_cast<uint32>(m_entries.Size()) & 0x000000ff));

        for (auto &command : m_entries) {
            // command name (enum / uint32)
            out.PushBack((static_cast<uint32>(command.command_name) & 0xff000000) >> 24);
            out.PushBack((static_cast<uint32>(command.command_name) & 0x00ff0000) >> 16);
            out.PushBack((static_cast<uint32>(command.command_name) & 0x0000ff00) >> 8);
            out.PushBack((static_cast<uint32>(command.command_name) & 0x000000ff));

            // command flags (byte)
            out.PushBack(command.flags);

            // string size (uint32) note we encode size not length, as we are counting in bytes
            out.PushBack((static_cast<uint32>(command.json_string.Size()) & 0xff000000) >> 24);
            out.PushBack((static_cast<uint32>(command.json_string.Size()) & 0x00ff0000) >> 16);
            out.PushBack((static_cast<uint32>(command.json_string.Size()) & 0x0000ff00) >> 8);
            out.PushBack((static_cast<uint32>(command.json_string.Size()) & 0x000000ff));

            for (SizeType i = 0; i < command.json_string.Size(); i++) {
                out.PushBack(static_cast<ubyte>(command.json_string[i]));
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
    ubyte m_flags;
    Array<CommandEntry> m_entries;
};

} // namespace hyperion

#endif