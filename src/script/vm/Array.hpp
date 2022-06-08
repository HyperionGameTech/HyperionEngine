#ifndef ARRAY_HPP
#define ARRAY_HPP

#include <script/vm/Value.hpp>

#include <system/debug.h>

#include <sstream>

namespace hyperion {
namespace vm {

class Array {
public:
    Array(size_t size = 0);
    Array(const Array &other);
    ~Array();

    Array &operator=(const Array &other);
    inline bool operator==(const Array &other) const { return this == &other; }

    inline size_t GetSize() const { return m_size; }
    inline Value *GetBuffer() const { return m_buffer; }
    inline Value &AtIndex(int index) { return m_buffer[index]; }
    inline const Value &AtIndex(int index) const { return m_buffer[index]; }
    inline void AtIndex(int index, const Value &value) { m_buffer[index] = value; }

    void Resize(size_t capacity);
    void Push(const Value &value);
    void PushMany(size_t n, Value *values);
    void PushMany(size_t n, Value **values);
    void Pop();

    void GetRepresentation(std::stringstream &ss, bool add_type_name = true) const;

private:
    size_t m_size;
    size_t m_capacity;
    Value *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
