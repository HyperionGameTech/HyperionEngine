#pragma once

#include <Core/FxRef.hpp>
#include <Core/FxTSRef.hpp>

template <typename T>
concept C_IsRefType = requires(T ref) {
    typename T::UserType;
    { ref.mpPtr } -> std::convertible_to<typename T::UserType*>;
    { ref.mpRefCnt->Inc() } -> std::same_as<void>;
};


/**
 * Constructs a new `FxRef` for the type `T` using the arguments `args`.
 * @tparam T the undelying type of the reference.
 * @tparam Types the types of the arguments passed in.
 */
template <typename T, typename... Types>
FxRef<T> FxMakeRef(Types... args)
{
    return FxRef<T>::New(std::forward<Types>(args)...);
}

/**
 * Constructs a new thread-safe reference counted object.
 * @tparam T the undelying type of the reference.
 * @tparam Types the types of the arguments passed in.
 */
template <typename T, typename... Types>
FxRef<T> FxMakeTSRef(Types... args)
{
    return FxRef<T>::New(std::forward<Types>(args)...);
}


template <C_IsRefType TRefType>
struct FxRefContext
{
public:
    using UserType = TRefType::UserType;

    FxRefContext(const TRefType& ref) : mRef(ref)
    {
        FxAssert(mRef.mpRefCnt != nullptr);
        mRef.mpRefCnt->Inc();
    }

    FxRefContext(const FxRefContext<TRefType>& other) = delete;
    FxRefContext(FxRefContext<TRefType>&& other) = delete;

    FX_FORCE_INLINE UserType* GetPtr() { return mRef.mpPtr; }
    FX_FORCE_INLINE UserType& Get() { return *mRef.mpPtr; }

    ~FxRefContext() { mRef.mpRefCnt->Dec(); }

private:
    const TRefType& mRef;
};
