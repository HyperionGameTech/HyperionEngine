/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scripting/ScriptFwd.hpp>

#include <core/Defines.hpp>

#include <core/reflection/ObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Types.hpp>

#ifdef HYP_SCRIPT
#include <core/reflection/BoxedValue.hpp>
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
    BoxedValue obj;
};
#endif

struct ScriptObjectData_Native final
{
    static constexpr ScriptLanguage Language = SL_NATIVE;

    WeakHandle<ObjectBase> nativeObject;
};

class HYP_API ScriptObjectResource final : public ResourceBase
{
public:
    ScriptObjectResource();

    explicit ScriptObjectResource(const Handle<ObjectBase>& nativeObject);

    ScriptObjectResource(dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(TypedObjPtr ptr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(TypedObjPtr ptr, dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(TypedObjPtr ptr, const RC<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);

#ifdef HYP_SCRIPT
    ScriptObjectResource(Script_Instance* hypScriptInstance, BoxedValue&& hypScriptValue);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    uint32 GetScriptLanguageMask() const;

    dotnet::ManagedObject* GetManagedObject() const;
    const RC<dotnet::ManagedClass> GetManagedClass() const;

    ScriptObjectData_Native* GetScriptObjectData_Native() const
    {
        return nativeData;
    }

    void SetScriptObjectData_Native(const ScriptObjectData_Native& data)
    {
        if (!nativeData)
        {
            nativeData = new ScriptObjectData_Native();
        }

        *nativeData = data;
    }

#ifdef HYP_DOTNET
    ScriptObjectData_DotNet* GetScriptObjectData_DotNet() const
    {
        return dotNetData;
    }

    void SetScriptObjectData_DotNet(const ScriptObjectData_DotNet& data)
    {
        if (!dotNetData)
        {
            dotNetData = new ScriptObjectData_DotNet();
        }

        *dotNetData = data;
    }
#endif

#ifdef HYP_SCRIPT
    ScriptObjectData_HypScript* GetScriptObjectData_HypScript() const
    {
        return hypScriptData;
    }

    void SetScriptObjectData_HypScript(const ScriptObjectData_HypScript& data)
    {
        if (!hypScriptData)
        {
            hypScriptData = new ScriptObjectData_HypScript();
        }

        *hypScriptData = data;
    }
#endif

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    TypedObjPtr m_ptr;

    struct
    {
        ScriptObjectData_Native* nativeData = nullptr;

#ifdef HYP_DOTNET
        ScriptObjectData_DotNet* dotNetData = nullptr;
#endif
#ifdef HYP_SCRIPT
        ScriptObjectData_HypScript* hypScriptData = nullptr;
#endif
    };
};

} // namespace hyperion
