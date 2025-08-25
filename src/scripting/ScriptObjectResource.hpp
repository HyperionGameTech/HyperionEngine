/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/Types.hpp>

#ifdef HYP_SCRIPT
#include <script/vm/Value.hpp>
#endif

namespace hyperion {

namespace dotnet {
class Class;
class Object;
class Method;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;
enum ScriptLanguage : uint32;

class HYP_API ScriptObjectResource final : public ResourceBase
{
public:
    ScriptObjectResource(dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);

#ifdef HYP_SCRIPT
    ScriptObjectResource(HypObjectPtr ptr, vm::Value&& value);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    ScriptLanguage GetScriptLanguage() const;

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

    dotnet::Object* m_objectPtr;
    RC<dotnet::Class> m_managedClass;

#ifdef HYP_SCRIPT
    vm::Value m_value;
#endif
};

} // namespace hyperion
