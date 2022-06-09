#ifndef INSTRUCTION_STREAM_HPP
#define INSTRUCTION_STREAM_HPP

#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>

#include <vector>
#include <ostream>
#include <cstdint>

namespace hyperion::compiler {

class InstructionStream {
    //friend std::ostream &operator<<(std::ostream &os, InstructionStream instruction_stream);
    
public:
    InstructionStream();
    InstructionStream(const InstructionStream &other);

    /*inline void ClearInstructions() { m_position = 0; m_data.clear(); }

    inline size_t GetPosition() const { return m_position; }
    inline size_t &GetPosition() { return m_position; }

    inline const std::vector<Instruction<>> &GetData() const { return m_data; }*/

    inline uint8_t GetCurrentRegister() const { return m_register_counter; }
    inline uint8_t IncRegisterUsage() { return ++m_register_counter; }
    inline uint8_t DecRegisterUsage() { return --m_register_counter; }

    inline int GetStackSize() const { return m_stack_size; }
    inline void SetStackSize(int stack_size) { m_stack_size = stack_size; }
    inline int IncStackSize() { return ++m_stack_size; }
    inline int DecStackSize() { return --m_stack_size; }

    inline int NewStaticId() { return m_static_id++; }

    inline void AddStaticObject(const StaticObject &static_object)
        { m_static_objects.push_back(static_object); }

    int FindStaticObject(const StaticObject &static_object) const;

    //InstructionStream &operator<<(const Instruction<> &instruction);

private:
    //size_t m_position;
    //std::vector<Instruction<>> m_data;
    // incremented and decremented each time a register
    // is used/unused
    uint8_t m_register_counter;
    // incremented each time a variable is pushed,
    // decremented each time a stack frame is closed
    int m_stack_size;
    // the current static object id
    int m_static_id;

    std::vector<StaticObject> m_static_objects;
};

} // namespace hyperion::compiler

#endif
