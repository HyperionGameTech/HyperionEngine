#include <script/vm/VMArray.hpp>

#include <core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {
namespace vm {

VMArray::VMArray(SizeType size)
{
    m_internalArray.Resize(size);
}

void VMArray::Resize(SizeType newSize)
{
    m_internalArray.Resize(newSize);
}

void VMArray::Push(Value&& value)
{
    m_internalArray.PushBack(std::move(value));
}

void VMArray::Pop()
{
    m_internalArray.PopBack();
}

void VMArray::GetRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
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
    for (SizeType i = 0; i < m_internalArray.Size(); i++)
    {
        m_internalArray[i].ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        if (i != m_internalArray.Size() - 1)
        {
            ss << sepStr;
        }
    }

    ss << ']';
}

} // namespace vm
} // namespace hyperion
