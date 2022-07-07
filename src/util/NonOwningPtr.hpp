#ifndef NON_OWNING_PTR_H
#define NON_OWNING_PTR_H

namespace hyperion {
template <class T>
// just a marker to state that this object is not to be deleted by the current scope
class non_owning_ptr {
public:
    non_owning_ptr() : ptr(nullptr) {}
    non_owning_ptr(std::nullptr_t) : ptr(nullptr) {}
    // marked explicit so that it is obvious in the code when this class is used
    explicit non_owning_ptr(T *ptr) : ptr(ptr) {}
    non_owning_ptr(const non_owning_ptr &other) : ptr(other.ptr) {}
    non_owning_ptr &operator=(const non_owning_ptr &other) { ptr = other.ptr; return *this; }
    non_owning_ptr &operator=(std::nullptr_t) { ptr = nullptr; return *this; }
    ~non_owning_ptr() = default;

    inline T &operator*() { return *ptr; }
    inline const T &operator*() const { return *ptr; }
    inline T *operator->() { return ptr; }
    inline const T *operator->() const { return ptr; }
    inline bool operator==(const non_owning_ptr &other) const { return ptr == other.ptr; }
    inline bool operator!=(const non_owning_ptr &other) const { return ptr != other.ptr; }
    inline bool operator==(const T *other) const { return ptr == other; }
    inline bool operator!=(const T *other) const { return ptr != other; }
    inline bool operator==(std::nullptr_t) const { return !ptr; }
    inline bool operator!=(std::nullptr_t) const { return !!ptr; }
    inline bool operator!() const { return !ptr; }
    inline operator bool() const { return ptr != nullptr; }

    inline T *get() { return ptr; }
    inline const T *get() const { return ptr; }

private:
    T *ptr;
};

template <class T>
static inline non_owning_ptr<T> make_non_owning(T *ptr)
{
    return non_owning_ptr(ptr);
}

} // namespace hyperion

#endif