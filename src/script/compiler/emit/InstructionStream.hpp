#ifndef INSTRUCTION_STREAM_HPP
#define INSTRUCTION_STREAM_HPP

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>

#include <Types.hpp>

#include <vector>
#include <ostream>
#include <cstdint>

namespace hyperion::compiler {

class InstructionStream
{
public:
    InstructionStream();
    InstructionStream(const InstructionStream &other);

    UInt8 GetCurrentRegister() const { return m_register_counter; }
    UInt8 IncRegisterUsage() { return ++m_register_counter; }
    UInt8 DecRegisterUsage() { return --m_register_counter; }

    Int GetStackSize() const
        { return m_stack_size; }

    void SetStackSize(Int stack_size)
        { m_stack_size = stack_size; }

    Int IncStackSize()
        { return ++m_stack_size; }

    Int DecStackSize()
    {
        AssertThrowMsg(m_stack_size > 0, "Compiler stack size record invalid");

        return --m_stack_size;
    }

    Int NewStaticId() { return m_static_id++; }

    void AddStaticObject(const StaticObject &static_object)
        { m_static_objects.PushBack(static_object); }

    Int FindStaticObject(const StaticObject &static_object) const;

private:
    // incremented and decremented each time a register
    // is used/unused
    UInt8 m_register_counter;
    // incremented each time a variable is pushed,
    // decremented each time a stack frame is closed
    Int m_stack_size;
    // the current static object id
    Int m_static_id;

    Array<StaticObject> m_static_objects;
};

} // namespace hyperion::compiler

#endif
