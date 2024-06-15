/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_PROPERTY_HPP
#define HYPERION_CORE_HYP_CLASS_PROPERTY_HPP

#include <core/HypClassPropertySerializer.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/functional/Proc.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/memory/Any.hpp>

#include <Types.hpp>

namespace hyperion {

#ifdef HYP_DEBUG_MODE
#define HYP_CLASS_PROPERTY_DEBUG_INFO
#endif

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
struct HypClassPropertyDebugInfo
{
    TypeID  return_type_id;
    TypeID  target_type_id;
    TypeID  value_type_id;
};
#endif

struct HypClassPropertyGetter
{
    Proc<Any, const void *>     proc;
    IHypClassPropertySerializer *serializer = nullptr;

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
    HypClassPropertyDebugInfo   debug_info;
#endif

    HypClassPropertyGetter() = default;

    template <class ReturnType, class TargetType>
    HypClassPropertyGetter(ReturnType(TargetType::*MemFn)())
        : proc([MemFn](const void *target) -> Any
          {
              return Any::Construct<ReturnType>(std::forward<ReturnType>((static_cast<const TargetType *>(target)->*MemFn)()));
          }),
          serializer(GetClassPropertySerializer<NormalizedType<ReturnType>>())
    {
#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        debug_info.return_type_id = TypeID::ForType<NormalizedType<ReturnType>>();
#endif
    }

    template <class ReturnType, class TargetType>
    HypClassPropertyGetter(ReturnType(TargetType::*MemFn)() const)
        : proc([MemFn](const void *target) -> Any
          {
              return Any::Construct<ReturnType>(std::forward<ReturnType>((static_cast<const TargetType *>(target)->*MemFn)()));
          }),
          serializer(GetClassPropertySerializer<NormalizedType<ReturnType>>())
    {
#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        debug_info.return_type_id = TypeID::ForType<NormalizedType<ReturnType>>();
#endif
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    explicit operator bool() const
        { return proc.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return proc.IsValid(); }

    template <class TargetType>
    Any Invoke(const TargetType &target) const
    {
        AssertThrow(IsValid());

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        AssertThrowMsg(TypeID::ForType<NormalizedType<TargetType>>() == debug_info.target_type_id, "Target type mismatch");
#endif

        return proc(&target);
    }

    template <class TargetType>
    Any Invoke(const TargetType *target) const
    {
        AssertThrow(target != nullptr);

        return Invoke<TargetType>(*target);
    }
};

struct HypClassPropertySetter
{
    Proc<void, void *, const void *>    proc;
    IHypClassPropertySerializer         *serializer = nullptr;

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
    HypClassPropertyDebugInfo           debug_info;
#endif

    HypClassPropertySetter() = default;

    template <class ReturnType, class TargetType, class ValueType>
    HypClassPropertySetter(ReturnType(TargetType::*MemFn)(ValueType))
        : proc([MemFn](void *target, const void *in) -> void
          {
              using ReturnTypePtr = std::add_pointer_t< std::add_const_t< NormalizedType< ValueType > > >;

              (static_cast<TargetType *>(target)->*MemFn)(*static_cast< ReturnTypePtr >(in));
          }),
          serializer(GetClassPropertySerializer<NormalizedType<ValueType>>())
    {
#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        debug_info.return_type_id = TypeID::ForType<ReturnType>();
        debug_info.value_type_id = TypeID::ForType<NormalizedType<ValueType>>();
#endif
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    explicit operator bool() const
        { return proc.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsValid() const
        { return proc.IsValid(); }

    template <class TargetType, class ValueType>
    void Invoke(TargetType &target, ValueType &&value) const
    {
        AssertThrow(IsValid());

#ifdef HYP_CLASS_PROPERTY_DEBUG_INFO
        AssertThrowMsg(TypeID::ForType<NormalizedType<TargetType>>() == debug_info.target_type_id, "Target type mismatch");
        AssertThrowMsg(TypeID::ForType<NormalizedType<ValueType>>() == debug_info.value_type_id, "Value type mismatch");
#endif

        proc(&target, &value);
    }

    template <class TargetType, class ValueType>
    void Invoke(TargetType *target, ValueType &&value) const
    {
        AssertThrow(target != nullptr);

        return Invoke<TargetType, ValueType>(*target, std::forward<ValueType>(value));
    }
};

struct HypClassProperty
{
    Name                    name;
    HypClassPropertyGetter  getter;
    HypClassPropertySetter  setter;

    HypClassProperty() = default;

    HypClassProperty(Name name)
        : name(name)
    {
    }

    HypClassProperty(Name name, HypClassPropertyGetter &&getter)
        : name(name),
          getter(std::move(getter))
    {
    }

    HypClassProperty(Name name, HypClassPropertyGetter &&getter, HypClassPropertySetter &&setter)
        : name(name),
          getter(std::move(getter)),
          setter(std::move(setter))
    {
    }

    HypClassProperty(const HypClassProperty &other)                 = delete;
    HypClassProperty &operator=(const HypClassProperty &other)      = delete;
    HypClassProperty(HypClassProperty &&other) noexcept             = default;
    HypClassProperty &operator=(HypClassProperty &&other) noexcept  = default;

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasGetter() const
        { return getter.IsValid(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasSetter() const
        { return setter.IsValid(); }
};

} // namespace hyperion

#endif