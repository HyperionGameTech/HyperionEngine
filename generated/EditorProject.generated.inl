#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region EditorProject Reflection Data

HYP_BEGIN_CLASS(EditorProject, 37, 0, NAME("HypObjectBase"))
    Method(NAME(HYP_STR(GetUUID)), &EditorProject::GetUUID),
    Method(NAME(HYP_STR(GetName)), &EditorProject::GetName),
    Method(NAME(HYP_STR(SetName)), &EditorProject::SetName),
    Method(NAME(HYP_STR(GetLastSavedTime)), &EditorProject::GetLastSavedTime),
    Method(NAME(HYP_STR(GetFilePath)), &EditorProject::GetFilePath),
    Method(NAME(HYP_STR(GetScenes)), &EditorProject::GetScenes, Span<const ClassAttribute> { {ClassAttribute("property", "Scenes") } }),
    Method(NAME(HYP_STR(GetPackage)), &EditorProject::GetPackage),
    Method(NAME(HYP_STR(AddScene)), &EditorProject::AddScene),
    Method(NAME(HYP_STR(RemoveScene)), &EditorProject::RemoveScene),
    Method(NAME(HYP_STR(GetProjectsDirectory)), &EditorProject::GetProjectsDirectory),
    Method(NAME(HYP_STR(IsSaved)), &EditorProject::IsSaved),
    Method(NAME(HYP_STR(Save)), &EditorProject::Save),
    Method(NAME(HYP_STR(SaveAs)), &EditorProject::SaveAs),
    Method(NAME(HYP_STR(GetNextDefaultProjectName)), &EditorProject::GetNextDefaultProjectName, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(GetActionStack)), &EditorProject::GetActionStack),
    Method(NAME(HYP_STR(Close)), &EditorProject::Close),
    Field(NAME(HYP_STR(OnSceneAdded)), &EditorProject::OnSceneAdded, offsetof(EditorProject, OnSceneAdded), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnSceneRemoved)), &EditorProject::OnSceneRemoved, offsetof(EditorProject, OnSceneRemoved), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnProjectSaved)), &EditorProject::OnProjectSaved, offsetof(EditorProject, OnProjectSaved), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(Uuid)), &EditorProject::m_uuid, offsetof(EditorProject, m_uuid), Span<const ClassAttribute> { {ClassAttribute("property", "UUID") } }),
    Field(NAME(HYP_STR(Name)), &EditorProject::m_name, offsetof(EditorProject, m_name), Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Field(NAME(HYP_STR(LastSavedTime)), &EditorProject::m_lastSavedTime, offsetof(EditorProject, m_lastSavedTime), Span<const ClassAttribute> { {ClassAttribute("property", "LastSavedTime") } }),
    Field(NAME(HYP_STR(Filepath)), &EditorProject::m_filepath, offsetof(EditorProject, m_filepath), Span<const ClassAttribute> { {ClassAttribute("property", "FilePath") } }),
    Field(NAME(HYP_STR(Scenes)), &EditorProject::m_scenes, offsetof(EditorProject, m_scenes), Span<const ClassAttribute> { {ClassAttribute("property", "Scenes") } }),
    Field(NAME(HYP_STR(Package)), &EditorProject::m_package, offsetof(EditorProject, m_package), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(ActionStack)), &EditorProject::m_actionStack, offsetof(EditorProject, m_actionStack), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion EditorProject Reflection Data

#pragma region EditorProject Scriptable Methods

Name EditorProject::GetNextDefaultProjectName(const String & defaultProjectName) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetNextDefaultProjectName");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Name>(method_ptr, defaultProjectName);
        }
    }

    return GetNextDefaultProjectName_Impl(defaultProjectName);
}
#pragma endregion EditorProject Scriptable Methods
} // namespace hyperion

