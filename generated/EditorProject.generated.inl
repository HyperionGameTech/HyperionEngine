#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region EditorProject Reflection Data

HYP_BEGIN_CLASS(EditorProject, 198, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetUUID)), &EditorProject::GetUUID),
    HypMethod(NAME(HYP_STR(GetName)), &EditorProject::GetName),
    HypMethod(NAME(HYP_STR(SetName)), &EditorProject::SetName),
    HypMethod(NAME(HYP_STR(GetLastSavedTime)), &EditorProject::GetLastSavedTime),
    HypMethod(NAME(HYP_STR(GetFilePath)), &EditorProject::GetFilePath),
    HypMethod(NAME(HYP_STR(GetScenes)), &EditorProject::GetScenes, Span<const HypClassAttribute> { {HypClassAttribute("property", "Scenes") } }),
    HypMethod(NAME(HYP_STR(GetPackage)), &EditorProject::GetPackage),
    HypMethod(NAME(HYP_STR(AddScene)), &EditorProject::AddScene),
    HypMethod(NAME(HYP_STR(RemoveScene)), &EditorProject::RemoveScene),
    HypMethod(NAME(HYP_STR(GetProjectsDirectory)), &EditorProject::GetProjectsDirectory),
    HypMethod(NAME(HYP_STR(IsSaved)), &EditorProject::IsSaved),
    HypMethod(NAME(HYP_STR(Save)), &EditorProject::Save),
    HypMethod(NAME(HYP_STR(SaveAs)), &EditorProject::SaveAs),
    HypMethod(NAME(HYP_STR(GetNextDefaultProjectName)), &EditorProject::GetNextDefaultProjectName, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(GetActionStack)), &EditorProject::GetActionStack),
    HypMethod(NAME(HYP_STR(Close)), &EditorProject::Close),
    HypField(NAME(HYP_STR(OnSceneAdded)), &EditorProject::OnSceneAdded, offsetof(EditorProject, OnSceneAdded), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnSceneRemoved)), &EditorProject::OnSceneRemoved, offsetof(EditorProject, OnSceneRemoved), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnProjectSaved)), &EditorProject::OnProjectSaved, offsetof(EditorProject, OnProjectSaved), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(Uuid)), &EditorProject::m_uuid, offsetof(EditorProject, m_uuid), Span<const HypClassAttribute> { {HypClassAttribute("property", "UUID") } }),
    HypField(NAME(HYP_STR(Name)), &EditorProject::m_name, offsetof(EditorProject, m_name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypField(NAME(HYP_STR(LastSavedTime)), &EditorProject::m_lastSavedTime, offsetof(EditorProject, m_lastSavedTime), Span<const HypClassAttribute> { {HypClassAttribute("property", "LastSavedTime") } }),
    HypField(NAME(HYP_STR(Filepath)), &EditorProject::m_filepath, offsetof(EditorProject, m_filepath), Span<const HypClassAttribute> { {HypClassAttribute("property", "FilePath") } }),
    HypField(NAME(HYP_STR(Scenes)), &EditorProject::m_scenes, offsetof(EditorProject, m_scenes), Span<const HypClassAttribute> { {HypClassAttribute("property", "Scenes") } }),
    HypField(NAME(HYP_STR(Package)), &EditorProject::m_package, offsetof(EditorProject, m_package), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(ActionStack)), &EditorProject::m_actionStack, offsetof(EditorProject, m_actionStack), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion EditorProject Reflection Data

#pragma region EditorProject Scriptable Methods

Name EditorProject::GetNextDefaultProjectName(const String & defaultProjectName) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetNextDefaultProjectName");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Name>(method_ptr, defaultProjectName);
        }
    }

    return GetNextDefaultProjectName_Impl(defaultProjectName);
}
#pragma endregion EditorProject Scriptable Methods
} // namespace hyperion

