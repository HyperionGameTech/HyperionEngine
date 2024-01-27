#ifndef VM_SCRIPT_VM_ARRAY_HPP
#define VM_SCRIPT_VM_ARRAY_HPP

#include <script/vm/Value.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <system/Debug.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class VMArray
{
public:
    using SizeType = UInt64;

    VMArray(SizeType size = 0);
    VMArray(const VMArray &other);
    VMArray &operator=(const VMArray &other);
    VMArray(VMArray &&other) noexcept;
    VMArray &operator=(VMArray &&other) noexcept;
    ~VMArray();

    bool operator==(const VMArray &other) const
        { return this == &other; }

    SizeType GetSize() const
        { return m_size; }

    Value *GetBuffer() const
        { return m_buffer; }

    Value &AtIndex(SizeType index)
        { return m_buffer[index]; }

    const Value &AtIndex(SizeType index) const
        { return m_buffer[index]; }

    void AtIndex(SizeType index, const Value &value)
        { m_buffer[index] = value; }

    void Resize(SizeType capacity);
    void Push(const Value &value);
    void PushMany(SizeType n, Value *values);
    void PushMany(SizeType n, Value **values);
    void Pop();

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

    HashCode GetHashCode() const;

private:
    static SizeType GetCapacityForSize(SizeType new_size)
    {
        return static_cast<SizeType>(1) <<
            static_cast<SizeType>(std::ceil(std::log(MathUtil::Max(new_size, 1)) / std::log(2.0)));
    }

    SizeType    m_size;
    SizeType    m_capacity;
    Value       *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
