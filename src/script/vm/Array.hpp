#ifndef ARRAY_HPP
#define ARRAY_HPP

#include <script/vm/Value.hpp>
#include <types.h>

#include <system/debug.h>

#include <sstream>

namespace hyperion {
namespace vm {

class Array {
public:
    using SizeType = UInt64;

    Array(SizeType size = 0);
    Array(const Array &other);
    ~Array();

    Array &operator=(const Array &other);
    inline bool operator==(const Array &other) const { return this == &other; }

    inline SizeType GetSize() const                    { return m_size; }
    inline Value *GetBuffer() const                    { return m_buffer; }
    inline Value &AtIndex(int index)                   { return m_buffer[index]; }
    inline const Value &AtIndex(int index) const       { return m_buffer[index]; }
    inline void AtIndex(int index, const Value &value) { m_buffer[index] = value; }

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
    SizeType m_size;
    SizeType m_capacity;
    Value   *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
