#include <script/vm/Array.hpp>

#include <system/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {
namespace vm {

Array::Array(SizeType size)
    : m_size(size),
      m_capacity(GetCapacityForSize(size)),
      m_buffer(new Value[m_capacity])
{
    for (SizeType i = 0; i < m_capacity; i++) {
        m_buffer[i].m_type = Value::NONE;
        m_buffer[i].m_value.user_data = nullptr;
    }
}

Array::Array(const Array &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(new Value[other.m_capacity])
{
    // copy all members
    for (SizeType i = 0; i < m_size; i++) {
        m_buffer[i] = other.m_buffer[i];
    }
}

Array::~Array()
{
    delete[] m_buffer;
}

Array &Array::operator=(const Array &other)
{
    if (&other == this) {
        return *this;
    }

    if (m_buffer) {
        delete[] m_buffer;
    }

    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer = new Value[other.m_capacity];

    // copy all objects
    for (SizeType i = 0; i < m_size; i++) {
        m_buffer[i] = other.m_buffer[i];
    }

    return *this;
}

void Array::Resize(SizeType capacity)
{
    // delete and copy all over again
    m_capacity = capacity;
    auto *new_buffer = new Value[m_capacity];

    AssertThrow(m_size <= m_capacity);

    // copy all objects into new buffer
    for (SizeType i = 0; i < m_size; i++) {
        new_buffer[i] = m_buffer[i];
    }

    for (SizeType i = m_size; i < m_capacity; i++) {
        new_buffer[i] = Value(Value::NONE, {.user_data = nullptr});
    }

    // delete old buffer
    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }
    // set internal buffer to the new one
    m_buffer = new_buffer;
}

void Array::Push(const Value &value)
{
    const SizeType index = m_size;

    if (m_size >= m_capacity) {
        Resize(GetCapacityForSize(m_size + 1));
    }

    // set item at index
    m_buffer[index] = value;
    m_size++;
}

void Array::PushMany(SizeType n, Value *values)
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

void Array::PushMany(SizeType n, Value **values)
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

void Array::Pop()
{
    m_size--;
}

void Array::GetRepresentation(
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

} // namespace vm
} // namespace hyperion
