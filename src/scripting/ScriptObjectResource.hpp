/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scripting/ScriptFwd.hpp>

#include <core/Defines.hpp>

#include <core/reflection/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Types.hpp>

#ifdef HYP_SCRIPT
#include <core/reflection/HypData.hpp>
#endif

namespace hyperion {

struct ScriptObjectData_Dummy final
{
private:
    ScriptObjectData_Dummy() = default;

public:
    static constexpr ScriptLanguage Language = SL_INVALID;
};

#ifdef HYP_DOTNET

struct ScriptObjectData_DotNet final
{
    static constexpr ScriptLanguage Language = SL_CSHARP;

    dotnet::ManagedObject* objectPtr = nullptr;
    RC<dotnet::ManagedClass> managedClass = nullptr;
};

#endif

#ifdef HYP_SCRIPT
struct ScriptObjectData_HypScript final
{
    static constexpr ScriptLanguage Language = SL_HYPSCRIPT;

    Script_Instance* instance = nullptr;
    HypData obj;
};
#endif

struct ScriptObjectData_Native final
{
    static constexpr ScriptLanguage Language = SL_NATIVE;

    WeakHandle<HypObjectBase> nativeObject;
};

class HYP_API ScriptObjectResource final : public ResourceBase
{
public:
    ScriptObjectResource();

    explicit ScriptObjectResource(const Handle<HypObjectBase>& nativeObject);

    ScriptObjectResource(dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);

#ifdef HYP_SCRIPT
    ScriptObjectResource(Script_Instance* hypScriptInstance, HypData&& hypScriptValue);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    ScriptLanguage GetScriptLanguage() const;

    HYP_FORCE_INLINE dotnet::ManagedObject* GetManagedObject() const
    {
#ifdef HYP_DOTNET
        if (ScriptObjectData_DotNet* data = GetScriptObjectData_DotNet())
        {
            return data->objectPtr;
        }
#endif

        return nullptr;
    }

    const RC<dotnet::ManagedClass> GetManagedClass() const
    {
#ifdef HYP_DOTNET
        if (ScriptObjectData_DotNet* data = GetScriptObjectData_DotNet())
        {
            return data->managedClass;
        }
#endif

        return nullptr;
    }

    ScriptObjectData_Native* GetScriptObjectData_Native() const
    {
        return m_scriptObjectData.Is<ScriptObjectData_Native>() ? &m_scriptObjectData.Get<ScriptObjectData_Native>() : nullptr;
    }

    ScriptObjectData_DotNet* GetScriptObjectData_DotNet() const
    {
#ifdef HYP_DOTNET
        return m_scriptObjectData.Is<ScriptObjectData_DotNet>() ? &m_scriptObjectData.Get<ScriptObjectData_DotNet>() : nullptr;
#else
        return nullptr;
#endif
    }

    ScriptObjectData_HypScript* GetScriptObjectData_HypScript() const
    {
#ifdef HYP_SCRIPT
        return m_scriptObjectData.Is<ScriptObjectData_HypScript>() ? &m_scriptObjectData.Get<ScriptObjectData_HypScript>() : nullptr;
#else
        return nullptr;
#endif
    }

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    HypObjectPtr m_ptr;

    mutable Variant<
#ifdef HYP_DOTNET
        ScriptObjectData_DotNet,
#endif
#ifdef HYP_SCRIPT
        ScriptObjectData_HypScript,
#endif
        ScriptObjectData_Native>
        m_scriptObjectData;
};

} // namespace hyperion
