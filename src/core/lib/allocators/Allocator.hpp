#ifndef HYPERION_V2_LIB_ALLOCATOR_HPP
#define HYPERION_V2_LIB_ALLOCATOR_HPP

namespace hyperion {

template <class Derived>
class Allocator
{
public:
    Allocator()                                 = default;
    Allocator(const Allocator &)                = delete;
    Allocator &operator=(const Allocator &)     = delete;
    Allocator(Allocator &&) noexcept            = delete;
    Allocator &operator=(Allocator &&) noexcept = delete;
    virtual ~Allocator()                        = default;

    template <class T, class ...Args>
    T *New(Args &&... args)
    {
        return static_cast<Derived *>(this)->template New<T>(std::forward<Args>(args)...);
    }

    template <class T>
    void Delete(T *ptr)
    {
        static_cast<Derived *>(this)->template Delete<T>(ptr);
    }
};

} // namespace hyperion

#endif