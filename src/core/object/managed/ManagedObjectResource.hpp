/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MANAGED_OBJECT_RESOURCE_HPP
#define HYPERION_MANAGED_OBJECT_RESOURCE_HPP

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
class Method;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;

class HYP_API ManagedObjectResource final : public ResourceBase
{
public:
    ManagedObjectResource(dotnet::Object *object_ptr);
    ManagedObjectResource(HypObjectPtr ptr);
    ManagedObjectResource(HypObjectPtr ptr, const dotnet::ObjectReference &object_reference, EnumFlags<ObjectFlags> object_flags);

    ManagedObjectResource(const ManagedObjectResource &other)                   = delete;
    ManagedObjectResource &operator=(const ManagedObjectResource &other)        = delete;

    ManagedObjectResource(ManagedObjectResource &&other) noexcept;
    ManagedObjectResource &operator=(ManagedObjectResource &&other) noexcept    = delete;

    virtual ~ManagedObjectResource() override;

    HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const
        { return m_object_ptr; }

    dotnet::Class *GetManagedClass() const;

protected:

    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    HypObjectPtr    m_ptr;
    dotnet::Object  *m_object_ptr;
};

} // namespace hyperion

#endif