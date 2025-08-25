/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/Types.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
class Method;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;
enum ScriptLanguage : uint32;

#ifdef HYP_SCRIPT
enum HypScriptObjectTag
{
    HYP_SCRIPT_OBJECT
};
#endif

class HYP_API ScriptObjectResource final : public ResourceBase
{
public:
    ScriptObjectResource(dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);

#ifdef HYP_SCRIPT
    ScriptObjectResource(HypObjectPtr ptr, HypScriptObjectTag);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    HYP_FORCE_INLINE ScriptLanguage GetScriptLanguage() const
    {
        return m_scriptLanguage;
    }

    HYP_FORCE_INLINE dotnet::Object* GetManagedObject() const
    {
        return m_objectPtr;
    }

    const RC<dotnet::Class> GetManagedClass() const
    {
        return m_managedClass;
    }

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    HypObjectPtr m_ptr;

    ScriptLanguage m_scriptLanguage;

    dotnet::Object* m_objectPtr;
    RC<dotnet::Class> m_managedClass;
};

} // namespace hyperion
