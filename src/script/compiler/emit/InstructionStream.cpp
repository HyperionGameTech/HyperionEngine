#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace Hyperion {

InstructionStream::InstructionStream()
    : // m_position(0),
      m_registerCounter(0),
      m_stackSize(0),
      m_staticId(0)
{
}

InstructionStream::InstructionStream(const InstructionStream& other)
    : // m_position(other.m_position),
      // m_data(other.m_data),
      m_registerCounter(other.m_registerCounter),
      m_stackSize(other.m_stackSize),
      m_staticId(other.m_staticId),
      m_staticObjects(other.m_staticObjects)
{
}

int InstructionStream::FindStaticObject(const StaticObject& staticObject) const
{
    for (const StaticObject& so : m_staticObjects)
    {
        if (so == staticObject)
        {
            return so.m_id;
        }
    }
    // not found
    return -1;
}

/*InstructionStream &InstructionStream::operator<<(const Instruction<> &instruction)
{
    m_data.PushBack(instruction);
    for (const Array<char> &operand : instruction.m_data) {
        m_position += operand.Size();
    }
    return *this;
}*/

} // namespace Hyperion
