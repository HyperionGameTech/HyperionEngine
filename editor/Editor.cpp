#include <HyperionEngine.hpp>
#include <system/CommandQueue.hpp>

namespace hyperion::editor {




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