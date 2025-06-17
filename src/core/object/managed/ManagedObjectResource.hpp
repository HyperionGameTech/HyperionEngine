/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MANAGED_OBJECT_RESOURCE_HPP
#define HYPERION_MANAGED_OBJECT_RESOURCE_HPP

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

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
    ManagedObjectResource(dotnet::Object* object_ptr, const RC<dotnet::Class>& managed_class);
    ManagedObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managed_class);
    ManagedObjectResource(HypObjectPtr ptr, dotnet::Object* object_ptr, const RC<dotnet::Class>& managed_class);
    ManagedObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managed_class, const dotnet::ObjectReference& object_reference, EnumFlags<ObjectFlags> object_flags);

    ManagedObjectResource(const ManagedObjectResource& other) = delete;
    ManagedObjectResource& operator=(const ManagedObjectResource& other) = delete;

    ManagedObjectResource(ManagedObjectResource&& other) noexcept;
    ManagedObjectResource& operator=(ManagedObjectResource&& other) noexcept = delete;

    virtual ~ManagedObjectResource() override;

    HYP_FORCE_INLINE dotnet::Object* GetManagedObject() const
    {
        return m_object_ptr;
    }

    const RC<dotnet::Class> GetManagedClass() const
    {
        return m_managed_class;
    }

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    HypObjectPtr m_ptr;
    dotnet::Object* m_object_ptr;
    RC<dotnet::Class> m_managed_class;
};

} // namespace hyperion

#endif