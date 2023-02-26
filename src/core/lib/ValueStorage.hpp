#ifndef HYPERION_V2_LIB_VALUE_STORAGE_HPP
#define HYPERION_V2_LIB_VALUE_STORAGE_HPP

#include <core/lib/CMemory.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

template <class T>
struct alignas(T) ValueStorage
{
    alignas(T) UByte data_buffer[sizeof(T)];

    template <class ...Args>
    T *Construct(Args &&... args)
    {
        Memory::Construct<T>(data_buffer, std::forward<Args>(args)...);

        return &Get();
    }

    void Destruct()
        { Memory::Destruct<T>(static_cast<void *>(data_buffer)); }

    T &Get()
        { return *reinterpret_cast<T *>(&data_buffer); }

    [[nodiscard]] const T &Get() const
        { return *reinterpret_cast<const T *>(&data_buffer); }

    void *GetPointer()
        { return &data_buffer[0]; }

    const void *GetPointer() const
        { return &data_buffer[0]; }
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

    T *GetPointer()
        { return static_cast<T *>(&data[0]); }

    const T *GetPointer() const
        { return static_cast<T *>(&data[0]); }

    void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }

    const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }

    constexpr SizeType Size() const
        { return Sz; }

    constexpr SizeType TotalSize() const
        { return Sz * sizeof(T); }
};

template <class T>
struct ValueStorageArray<T, 0>
{
    ValueStorage<char> data[1];

    void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }

    const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }

    constexpr SizeType Size() const
        { return 0; }

    constexpr SizeType TotalSize() const
        { return 0; }
};

static_assert(sizeof(ValueStorageArray<int, 200>) == sizeof(int) * 200);
static_assert(sizeof(ValueStorageArray<int, 0>) == 1);

template <class To, class From>
static HYP_FORCE_INLINE To BitCast(const From &from)
{
    static_assert(std::is_standard_layout_v<From>, "From type must be standard layout");
    static_assert(std::is_standard_layout_v<To>, "To type must be standard layout");
    static_assert(sizeof(To) == sizeof(From), "sizeof must match for bit cast");

    ValueStorage<To> to_memory;

    ValueStorage<From> from_memory;
    Memory::MemCpy(&from_memory.data_buffer[0], &from, sizeof(From));

    for (SizeType i = 0; i < sizeof(from); i++) {
        to_memory.data_buffer[i] = from_memory.data_buffer[i];
    }

    return to_memory.Get();
}

} // namespace hyperion

#endif