#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace hyperion::compiler {

InstructionStream::InstructionStream()
    : //m_position(0),
      m_register_counter(0),
      m_stack_size(0),
      m_static_id(0)
{
}

InstructionStream::InstructionStream(const InstructionStream &other)
    : //m_position(other.m_position),
      //m_data(other.m_data),
      m_register_counter(other.m_register_counter),
      m_stack_size(other.m_stack_size),
      m_static_id(other.m_static_id),
      m_static_objects(other.m_static_objects)
{
}

Int InstructionStream::FindStaticObject(const StaticObject &static_object) const
{
    for (const StaticObject &so : m_static_objects) {
        if (so == static_object) {
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

} // namespace hyperion::compiler
