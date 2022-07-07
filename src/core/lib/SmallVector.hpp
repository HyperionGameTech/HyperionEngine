#ifndef HYPERION_SMALL_VECTOR_H
#define HYPERION_SMALL_VECTOR_H

#include <math/MathUtil.hpp>
#include <Util.hpp>

namespace hyperion {

#if 0

template <class T, size_t ReservedSize = 4096 / sizeof(T)>
class SmallVector
{
public:
    SmallVector() = default;
    ~SmallVector()
    {
        if (m_dynamic_storage != nullptr) {
            delete[] m_dynamic_storage;
        }
    }

    HYP_FORCE_INLINE T &operator[](size_t index)             { return m_head[index]; }
    HYP_FORCE_INLINE const T &operator[](size_t index) const { return m_head[index]; }

    HYP_FORCE_INLINE T *Data() const { return m_head; }
    HYP_FORCE_INLINE size_t Size() const { return m_size; }

    void PushBack(const T &item)
    {
        const size_t new_size = m_size + 1;

        if (m_head == &m_reserved_storage[0]) {
            if (new_size > ReservedSize) {
                AssertThrow(m_dynamic_storage == nullptr);

                m_capacity = NewCapacity(new_size);

                /* TODO: placement new / MOVE ? */
                m_dynamic_storage = new T[m_capacity];
                std::memcpy(&m_dynamic_storage[0], &m_reserved_storage[0], sizeof(T) * m_size);

                m_head = m_dynamic_storage;
            }
        } else if (new_size > m_capacity) {
            m_capacity = NewCapacity(new_size);

            T *new_dynamic_storage = new T[m_capacity];

            if (m_dynamic_storage != nullptr) {
                /* TODO: placement new / MOVE ? */
                std::memcpy(new_dynamic_storage, m_dynamic_storage, m_size * sizeof(T));
                delete[] m_dynamic_storage;
            }

            m_dynamic_storage = new_dynamic_storage;
        }

        m_size = new_size;
        m_head[new_size - 1] = item;
    }

    void PopBack()
    {
        AssertThrow(m_size != 0);

        const size_t new_size = m_size - 1;
        size_t reduced_capacity = NewCapacity(new_size);

        if (m_head == m_dynamic_storage) {
            if (reduced_capacity < m_capacity) {
                std::destroy(m_dynamic_storage + m_size, m_capacity);
            }
        }

        m_size = new_size;

    }

private:
    inline size_t NewCapacity(size_t new_size) const
    {
        return size_t(1) << MathUtil::Ceil(std::log(new_size) / std::log(2.0));
    }

    T m_reserved_storage[ReservedSize];
    T *m_dynamic_storage = nullptr;
    T *m_head            = &m_reserved_storage[0];
    size_t m_size        = 0;
    size_t m_capacity    = 0;
};

#endif

} // namespace hyperion

#endif