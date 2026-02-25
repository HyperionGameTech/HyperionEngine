/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scripting/ScriptFwd.hpp>

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectFwd.hpp>

#include <Core/memory/resource/Resource.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Variant.hpp>

#include <Core/Types.hpp>

#ifdef HYP_SCRIPT
#include <Core/reflection/BoxedValue.hpp>
#endif

namespace Hyperion {

struct ScriptObjectData_Dummy final
{
private:
    ScriptObjectData_Dummy() = default;

public:
    static constexpr ScriptLanguage Language = ScriptLanguage::Invalid;
};

#ifdef HYP_DOTNET

struct ScriptObjectData_DotNet final
{
    static constexpr ScriptLanguage Language = ScriptLanguage::CSharp;

    dotnet::ManagedObject* objectPtr = nullptr;
    RC<dotnet::ManagedClass> managedClass = nullptr;
};

#endif

#ifdef HYP_SCRIPT
struct ScriptObjectData_HypScript final
{
    static constexpr ScriptLanguage Language = ScriptLanguage::HypScript;

    ScriptInstance* instance = nullptr;
    BoxedValue obj;
};
#endif

struct ScriptObjectData_Native final
{
    static constexpr ScriptLanguage Language = ScriptLanguage::Native;

    WeakHandle<ObjectBase> nativeObject;
};

class ScriptObjectResource final : public ResourceBase
{
public:
    ScriptObjectResource();

    explicit ScriptObjectResource(const Handle<ObjectBase>& nativeObject);

    ScriptObjectResource(dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, dotnet::ManagedObject* objectPtr, const RC<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, const RC<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);

#ifdef HYP_SCRIPT
    ScriptObjectResource(ScriptInstance* hypScriptInstance, BoxedValue&& hypScriptValue);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    uint32 GetScriptLanguageMask() const;

    dotnet::ManagedObject* GetManagedObject() const;
    const RC<dotnet::ManagedClass> GetManagedClass() const;

    ScriptObjectData_Native* GetScriptObjectData_Native()
    {
        return nativeData.TryGet();
    }

    const ScriptObjectData_Native* GetScriptObjectData_Native() const
    {
        return nativeData.TryGet();
    }

    void SetScriptObjectData_Native(const ScriptObjectData_Native& data)
    {
        nativeData = data;
    }

#ifdef HYP_DOTNET
    ScriptObjectData_DotNet* GetScriptObjectData_DotNet()
    {
        return dotNetData.TryGet();
    }

    const ScriptObjectData_DotNet* GetScriptObjectData_DotNet() const
    {
        return dotNetData.TryGet();
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
    ScriptObjectData_HypScript* GetScriptObjectData_HypScript()
    {
        return hypScriptData.TryGet();
    }

    const ScriptObjectData_HypScript* GetScriptObjectData_HypScript() const
    {
        return hypScriptData.TryGet();
    }

    void SetScriptObjectData_HypScript(const ScriptObjectData_HypScript& data)
    {
        hypScriptData = data;
    }
#endif

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    ObjectBase* m_ptr;

    Optional<ScriptObjectData_Native> nativeData;
#ifdef HYP_DOTNET
    Optional<ScriptObjectData_DotNet> dotNetData;
#endif
#ifdef HYP_SCRIPT
    Optional<ScriptObjectData_HypScript> hypScriptData;
#endif
};

} // namespace Hyperion
