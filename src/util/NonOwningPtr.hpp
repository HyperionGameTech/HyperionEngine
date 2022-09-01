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

    T &operator*() { return *ptr; }
    const T &operator*() const { return *ptr; }
    T *operator->() { return ptr; }
    const T *operator->() const { return ptr; }
    bool operator==(const non_owning_ptr &other) const { return ptr == other.ptr; }
    bool operator!=(const non_owning_ptr &other) const { return ptr != other.ptr; }
    bool operator==(const T *other) const { return ptr == other; }
    bool operator!=(const T *other) const { return ptr != other; }
    bool operator==(std::nullptr_t) const { return !ptr; }
    bool operator!=(std::nullptr_t) const { return !!ptr; }
    bool operator!() const { return !ptr; }
    operator bool() const { return ptr != nullptr; }

    T *get() { return ptr; }
    const T *get() const { return ptr; }

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