#ifndef ARRAY_HPP
#define ARRAY_HPP

#include <script/vm/Value.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <system/Debug.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class Array
{
public:
    using SizeType = UInt64;

    Array(SizeType size = 0);
    Array(const Array &other);
    ~Array();

    Array &operator=(const Array &other);
    bool operator==(const Array &other) const { return this == &other; }

    SizeType GetSize() const                    { return m_size; }
    Value *GetBuffer() const                    { return m_buffer; }
    Value &AtIndex(int index)                   { return m_buffer[index]; }
    const Value &AtIndex(int index) const       { return m_buffer[index]; }
    void AtIndex(int index, const Value &value) { m_buffer[index] = value; }

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

private:
    static inline SizeType GetCapacityForSize(SizeType new_size)
    {
        return static_cast<SizeType>(1) <<
            static_cast<SizeType>(std::ceil(std::log(MathUtil::Max(new_size, 1)) / std::log(2.0)));
    }

    SizeType m_size;
    SizeType m_capacity;
    Value *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
