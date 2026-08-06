/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scripting/ScriptFwd.hpp>

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Resource/Resource.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Variant.hpp>
#include <Core/Utilities/Optional.hpp>

#include <Core/Util.hpp>
#include <Core/Types.hpp>

#ifdef HYP_SCRIPT
#include <Core/Reflection/BoxedValue.hpp>
#endif // HYP_SCRIPT

#ifdef HYP_STRATA_JIT
struct StrataJit;
#endif // HYP_STRATA_JIT

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
    SharedPtr<dotnet::ManagedClass> managedClass = nullptr;
};
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
struct ScriptObjectData_HypScript final
{
    static constexpr ScriptLanguage Language = ScriptLanguage::HypScript;

    ScriptInstance* instance = nullptr;
    ObjectBase* obj = nullptr;
};
#endif // HYP_SCRIPT

struct ScriptObjectData_Strata final
{
    static constexpr ScriptLanguage Language = ScriptLanguage::Strata;

    StringHash moduleHash;

#ifdef HYP_STRATA_JIT
    StrataJit* jit = nullptr;
#endif // HYP_STRATA_JIT
};

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

#ifdef HYP_DOTNET
    ScriptObjectResource(dotnet::ManagedObject* objectPtr, const SharedPtr<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, const SharedPtr<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, dotnet::ManagedObject* objectPtr, const SharedPtr<dotnet::ManagedClass>& managedClass);
    ScriptObjectResource(ObjectBase* ptr, const SharedPtr<dotnet::ManagedClass>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
    ScriptObjectResource(ScriptInstance* hypScriptInstance, ObjectBase* hypScriptValue);
#endif // HYP_SCRIPT

#ifdef HYP_STRATA
    ScriptObjectResource(ValueWrapper<ScriptLanguage::Strata>, StringHash moduleHash);
#endif // HYP_STRATA

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    uint32 GetScriptLanguageMask() const;

    dotnet::ManagedObject* GetManagedObject() const;
    const SharedPtr<dotnet::ManagedClass> GetManagedClass() const;

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
#endif // HYP_DOTNET

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
#endif // HYP_SCRIPT
    
#ifdef HYP_STRATA
    ScriptObjectData_Strata* GetScriptObjectData_Strata()
    {
        return strataData.TryGet();
    }

    const ScriptObjectData_Strata* GetScriptObjectData_Strata() const
    {
        return strataData.TryGet();
    }

    void SetScriptObjectData_Strata(const ScriptObjectData_Strata& data)
    {
        strataData = data;
    }
#endif // HYP_STRATA

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    ObjectBase* m_ptr;

    Optional<ScriptObjectData_Native> nativeData;

#ifdef HYP_DOTNET
    Optional<ScriptObjectData_DotNet> dotNetData;
#endif // HYP_DOTNET

#ifdef HYP_SCRIPT
    Optional<ScriptObjectData_HypScript> hypScriptData;
#endif // HYP_SCRIPT
    
#ifdef HYP_STRATA
    Optional<ScriptObjectData_Strata> strataData;
#endif // HYP_STRATA
};

#ifdef HYP_DOTNET
ENGINE_API extern void Object_IncScriptObjectRef(ObjectBase* ptr);
ENGINE_API extern void Object_DecScriptObjectRef(ObjectBase* ptr);
#endif

} // namespace Hyperion
