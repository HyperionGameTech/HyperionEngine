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
    //friend std::ostream &operator<<(std::ostream &os, InstructionStream instruction_stream);
    
public:
    InstructionStream();
    InstructionStream(const InstructionStream &other);

    /*inline void ClearInstructions() { m_position = 0; m_data.clear(); }

    size_t GetPosition() const { return m_position; }
    size_t &GetPosition() { return m_position; }

    const Array<Instruction<>> &GetData() const { return m_data; }*/

    UInt8 GetCurrentRegister() const { return m_register_counter; }
    UInt8 IncRegisterUsage() { return ++m_register_counter; }
    UInt8 DecRegisterUsage() { return --m_register_counter; }

    int GetStackSize() const { return m_stack_size; }
    void SetStackSize(int stack_size) { m_stack_size = stack_size; }
    int IncStackSize() { return ++m_stack_size; }
    int DecStackSize() { return --m_stack_size; }

    int NewStaticId() { return m_static_id++; }

    void AddStaticObject(const StaticObject &static_object)
        { m_static_objects.PushBack(static_object); }

    int FindStaticObject(const StaticObject &static_object) const;

    //InstructionStream &operator<<(const Instruction<> &instruction);

private:
    //size_t m_position;
    //Array<Instruction<>> m_data;
    // incremented and decremented each time a register
    // is used/unused
    UInt8 m_register_counter;
    // incremented each time a variable is pushed,
    // decremented each time a stack frame is closed
    int m_stack_size;
    // the current static object id
    int m_static_id;

    Array<StaticObject> m_static_objects;
};

} // namespace hyperion::compiler

#endif
