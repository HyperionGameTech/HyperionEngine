#include <script/vm/VMArray.hpp>

#include <system/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {
namespace vm {

VMArray::VMArray(SizeType size)
    : m_size(size),
      m_capacity(GetCapacityForSize(size)),
      m_buffer(new Value[m_capacity])
{
    for (SizeType index = 0; index < m_capacity; index++) {
        m_buffer[index].m_type = Value::NONE;
        m_buffer[index].m_value.user_data = nullptr;
    }
}

VMArray::VMArray(const VMArray &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(new Value[other.m_capacity])
{
    // copy all members
    for (SizeType index = 0; index < m_size; index++) {
        m_buffer[index] = other.m_buffer[index];
    }
}

VMArray &VMArray::operator=(const VMArray &other)
{
    if (&other == this) {
        return *this;
    }

    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }

    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer = new Value[other.m_capacity];

    // copy all objects
    for (SizeType index = 0; index < m_size; index++) {
        m_buffer[index] = other.m_buffer[index];
    }

    return *this;
}

VMArray::VMArray(VMArray &&other) noexcept
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(other.m_buffer)
{
    other.m_size = 0;
    other.m_capacity = 0;
    other.m_buffer = nullptr;
}

VMArray &VMArray::operator=(VMArray &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }

    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer = other.m_buffer;

    other.m_size = 0;
    other.m_capacity = 0;
    other.m_buffer = nullptr;

    return *this;
}

VMArray::~VMArray()
{
    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }
}

void VMArray::Resize(SizeType capacity)
{
    // delete and copy all over again
    m_capacity = capacity;
    auto *new_buffer = new Value[m_capacity];

    AssertThrow(m_size <= m_capacity);

    // copy all objects into new buffer
    for (SizeType index = 0; index < m_size; index++) {
        new_buffer[index] = m_buffer[index];
    }

    for (SizeType index = m_size; index < m_capacity; index++) {
        new_buffer[index] = Value(Value::NONE, { .user_data = nullptr });
    }

    // delete old buffer
    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }
    // set internal buffer to the new one
    m_buffer = new_buffer;
}

void VMArray::Push(const Value &value)
{
    const SizeType index = m_size;

    if (m_size >= m_capacity) {
        Resize(GetCapacityForSize(m_size + 1));
    }

    // set item at index
    m_buffer[index] = value;
    m_size++;
}

void VMArray::PushMany(SizeType n, Value *values)
{
    const SizeType index = m_size;

    if (m_size + n >= m_capacity) {
        // delete and copy all over again
        Resize(GetCapacityForSize(m_size + n));
    }

    for (SizeType i = 0; i < n; i++) {
        // set item at index
        m_buffer[index + i] = values[i];
    }

    m_size += n;
}

void VMArray::PushMany(SizeType n, Value **values)
{
    const SizeType index = m_size;

    if (m_size + n >= m_capacity) {
        // delete and copy all over again
        Resize(GetCapacityForSize(m_size + n));
    }

    for (SizeType i = 0; i < n; i++) {
        AssertThrow(values[i] != nullptr);
        // set item at index
        m_buffer[index + i] = *values[i];
    }

    m_size += n;
}

void VMArray::Pop()
{
    m_size--;
}

void VMArray::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    if (depth == 0) {
        ss << "[...]";

        return;
    }

    // convert array list to string
    const char sep_str[3] = ", ";

    ss << '[';

    // convert all array elements to string
    for (SizeType i = 0; i < m_size; i++) {
        m_buffer[i].ToRepresentation(
            ss,
            add_type_name,
            depth - 1
        );

        if (i != m_size - 1) {
            ss << sep_str;
        }
    }

    ss << ']';
}

HashCode VMArray::GetHashCode() const
{
    HashCode hash_code = 0;

    for (SizeType i = 0; i < m_size; i++) {
        hash_code.Add(m_buffer[i].GetHashCode());
    }

    return hash_code;
}

} // namespace vm
} // namespace hyperion
