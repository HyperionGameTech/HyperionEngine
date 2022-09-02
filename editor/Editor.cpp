#include <HyperionEngine.hpp>

namespace hyperion::editor {


enum class CommandName : UInt
{

};

struct CommandEntry
{
    CommandName command_name;
    DynArray<String> args;
};

class CommandQueue
{
public:
    static constexpr UInt8 magic = 0xAE;

    static bool ReadCommandQueue(const UByte *data, SizeType size, CommandQueue &out)
    {
        UInt index = 0u;

        if (size < 5) { // one byte for magic, 4 for size are minimum.
            return false;
        }

        if (data[index++] != magic) {
            return false;
        }

        // next 4 bytes = size of list in COMMANDS
        union {
            UByte size_bytes[4];
            UInt32 size_uint;
        };

        for (UInt i = 0; i < 4; i++) {
            size_bytes[i] = data[index++];
        }

        if (size_uint == 0) {
            return true; // was valid, but no items exist
        }

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
                command_name_bytes[i] = data[index++];
            }

            command.command_name = static_cast<CommandName>(command_name_uint);

            // now we read number of args
            union {
                UByte num_args_bytes[4];
                UInt32 num_args_uint;
            };

            for (UInt i = 0; i < 4; i++) {
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
                    string_length_bytes[i] = data[index++];
                }

                if (string_length_uint != 0) {
                    char *str = new char[string_length_uint + 1];

                    for (UInt i = 0; i < string_length_uint; i++) {
                        string_length_bytes[i] = data[index++];
                    }

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

private:
    DynArray<CommandEntry> m_entries;
};

} // namespace hyperion::editor

int main(int argc, char *argv[])
{
    using namespace hyperion;
    using namespace hyperion::editor;

    if (argc > 1) {
        CommandQueue command_queue;

        auto decoded = String::Base64Decode(argv[1]);

        HYP_BREAKPOINT;
    }

    return 0;
}