#ifndef STATIC_MEMORY_HPP
#define STATIC_MEMORY_HPP

#include <script/vm/Value.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <utility>

namespace hyperion {
namespace vm {

class StaticMemory
{
public:
    static const UInt16 static_size;

public:
    StaticMemory();
    StaticMemory(const StaticMemory &other) = delete;
    ~StaticMemory();

    /*! \brief Marks all values for deallocation, 
        allowing the garbage collector to free them. */
    void MarkAllForDeallocation();
    
    HYP_FORCE_INLINE
    Value &operator[](SizeType index)
    {
        AssertThrowMsg(index < static_size, "out of bounds");
        return m_data[index];
    }
    
    HYP_FORCE_INLINE
    const Value &operator[](SizeType index) const
    {
        AssertThrowMsg(index < static_size, "out of bounds");
        return m_data[index];
    }

private:
    Value   *m_data;
};

} // namespace vm
} // namespace hyperion

#endif
