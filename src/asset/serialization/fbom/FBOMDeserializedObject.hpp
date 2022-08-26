#ifndef HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP
#define HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>
#include <string>
#include <map>

namespace hyperion::v2::fbom {

class FBOMDeserializedObject : public AtomicRefCountedPtr<void>
{
protected:
    using Base = AtomicRefCountedPtr<void>;

public:
    FBOMDeserializedObject()
        : Base()
    {
    }

    FBOMDeserializedObject(const FBOMDeserializedObject &other)
        : Base(other)
    {
    }

    FBOMDeserializedObject &operator=(const FBOMDeserializedObject &other)
    {
        Base::operator=(other);

        return *this;
    }

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept
        : Base(std::forward<FBOMDeserializedObject>(other))
    {
    }

    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept
    {
        Base::operator=(std::forward<FBOMDeserializedObject>(other));

        return *this;
    }

    ~FBOMDeserializedObject() = default;

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    auto Get() -> NormalizedType<T>&
    {
        AssertThrow(Base::Get() != nullptr);
        AssertThrow(Base::GetTypeID() == TypeID::ForType<NormalizedType<T>>());

        return *Base::Cast<NormalizedType<T>>();
    }

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    auto Get() const -> const NormalizedType<T>&
    {
        return const_cast<FBOMDeserializedObject *>(this)->template Get<T>();
    }

    template <class T>
    void Set(T &&value)
    {
        Base::template Set<NormalizedType<T>>(std::move(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically by the Any. */
    template <class T>
    void Reset(T *ptr)
    {
        Base::template Reset<NormalizedType<T>>(ptr); // take ownership
    }

    /*! \brief Drops ownership of the object held.
        Be sure to call delete on it when no longer needed!

        If no value is currently held, no changes to the underlying object will
        occur. Otherwise, the held value is set to nullptr, and nullptr is returned.
    */
    // template <class T>
    // [[nodiscard]] T *Release()
    // {
    //     // AssertThrow(false);
    //     return nullptr;
    //     // auto *ptr = RefCountedPtr<void>::Get();
        
    //     // if (!ptr) {
    //     //     return nullptr;
    //     // }

    //     // auto *ptr = any_ptr->Release<NormalizedType<T>>();

    //     // RefCountedPtr<Any>::Reset();

    //     // return held_ptr;
    // }
};

} // namespace hyperion::v2::fbom

#endif
