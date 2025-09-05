#include <script/vm/Array.hpp>

namespace hyperion {

void GetRepresentation(
    const Script_ValueArray& value,
    std::stringstream& ss,
    bool addTypeName,
    int depth)
{
    if (depth == 0)
    {
        ss << "[...]";

        return;
    }

    // convert array list to string
    const char sepStr[3] = ", ";

    ss << '[';

    // convert all array elements to string
    for (SizeType i = 0; i < value.Size(); i++)
    {
        value[i].ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        if (i != value.Size() - 1)
        {
            ss << sepStr;
        }
    }

    ss << ']';
}

} // namespace hyperion
