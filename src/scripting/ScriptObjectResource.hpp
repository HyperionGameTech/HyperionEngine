/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scripting/ScriptFwd.hpp>

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Types.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

namespace hyperion {

#ifdef HYP_SCRIPT
enum HypScriptObjectTag
{
    HYP_SCRIPT_OBJECT
};
#endif

struct ScriptObjectData_Dummy final
{
private:
    ScriptObjectData_Dummy() = default;

public:
    static constexpr ScriptLanguage lang = SL_INVALID;
};

#ifdef HYP_DOTNET
struct ScriptObjectData_DotNet final
{
    static constexpr ScriptLanguage lang = SL_CSHARP;

    dotnet::Object* objectPtr = nullptr;
    RC<dotnet::Class> managedClass = nullptr;
};
#endif

#ifdef HYP_SCRIPT
struct ScriptObjectData_HypScript final
{
    static constexpr ScriptLanguage lang = SL_HYPSCRIPT;

    Script_ObjectHandle objectHandle = INVALID_OBJECT;
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
    ScriptObjectResource(HypObjectPtr ptr, const Script_ObjectHandle& objectHandle, HypScriptObjectTag);
#endif

    ScriptObjectResource(const ScriptObjectResource& other) = delete;
    ScriptObjectResource& operator=(const ScriptObjectResource& other) = delete;

    ScriptObjectResource(ScriptObjectResource&& other) noexcept;
    ScriptObjectResource& operator=(ScriptObjectResource&& other) noexcept = delete;

    ~ScriptObjectResource();

    ScriptLanguage GetScriptLanguage() const;

    HYP_FORCE_INLINE dotnet::Object* GetManagedObject() const
    {
#ifdef HYP_DOTNET
        if (ScriptObjectData_DotNet* data = GetScriptObjectData_DotNet())
        {
            return data->objectPtr;
        }
#endif

        return nullptr;
    }

    const RC<dotnet::Class> GetManagedClass() const
    {
#ifdef HYP_DOTNET
        if (ScriptObjectData_DotNet* data = GetScriptObjectData_DotNet())
        {
            return data->managedClass;
        }
#endif

        return nullptr;
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
        ScriptObjectData_Dummy>
        m_scriptObjectData;

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
};

} // namespace hyperion
