#ifndef HYPERION_V2_LIB_VALUE_STORAGE_HPP
#define HYPERION_V2_LIB_VALUE_STORAGE_HPP

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

template <class T>
struct alignas(T) ValueStorage
{
    alignas(T) UByte data_buffer[sizeof(T)];

    T &Get()
    {
        return *reinterpret_cast<T *>(&data_buffer);
    }

    [[nodiscard]] const T &Get() const
    {
        return *reinterpret_cast<const T *>(&data_buffer);
    }
};

struct alignas(16) Tmp { int x; float y; void *stuff[16]; };
static_assert(sizeof(ValueStorage<Tmp>) == sizeof(Tmp));
static_assert(alignof(ValueStorage<Tmp>) == alignof(Tmp));

template <class T, SizeType Sz>
struct ValueStorageArray
{
    ValueStorage<T> data[Sz];

    ValueStorage<T> &operator[](SizeType index)
        { return data[index]; }

    const ValueStorage<T> &operator[](SizeType index) const
        { return data[index]; }
};

template <class T>
struct ValueStorageArray<T, 0>
{
    ValueStorage<char> data[1];
};

static_assert(sizeof(ValueStorageArray<void *, 200>) == sizeof(void *) * 200);
static_assert(sizeof(ValueStorageArray<void *, 0>) == 1);

} // namespace hyperion

#endif