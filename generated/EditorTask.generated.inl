#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EditorTaskBase Reflection Data

HYP_BEGIN_CLASS(EditorTaskBase, 201, 3, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(IsCommitted)), &EditorTaskBase::IsCommitted),
    HypMethod(NAME(HYP_STR(Cancel)), &EditorTaskBase::Cancel),
    HypMethod(NAME(HYP_STR(IsCompleted)), &EditorTaskBase::IsCompleted),
    HypMethod(NAME(HYP_STR(Process)), &EditorTaskBase::Process),
    HypMethod(NAME(HYP_STR(Commit)), &EditorTaskBase::Commit),
    HypField(NAME(HYP_STR(OnComplete)), &EditorTaskBase::OnComplete, offsetof(EditorTaskBase, OnComplete), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } }),
    HypField(NAME(HYP_STR(OnCancel)), &EditorTaskBase::OnCancel, offsetof(EditorTaskBase, OnCancel), Span<const HypClassAttribute> { {HypClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorTaskBase Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region LongRunningEditorTask Reflection Data

HYP_BEGIN_CLASS(LongRunningEditorTask, 202, 0, NAME("EditorTaskBase"), HypClassAttribute("abstract", true),HypClassAttribute("description", "A task that runs on a Task thread and has Process called one time only"))
    HypMethod(NAME(HYP_STR(IsCommitted)), &LongRunningEditorTask::IsCommitted),
    HypMethod(NAME(HYP_STR(Cancel)), &LongRunningEditorTask::Cancel, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(IsCompleted)), &LongRunningEditorTask::IsCompleted, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(Process)), &LongRunningEditorTask::Process, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(Commit)), &LongRunningEditorTask::Commit)
HYP_END_CLASS

#pragma endregion LongRunningEditorTask Reflection Data

#pragma region LongRunningEditorTask Scriptable Methods

void LongRunningEditorTask::Cancel()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Cancel");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    Cancel_Impl();
}
bool LongRunningEditorTask::IsCompleted() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("IsCompleted");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr);
        }
    }

    return IsCompleted_Impl();
}
void LongRunningEditorTask::Process()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Process");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    Process_Impl();
}
#pragma endregion LongRunningEditorTask Scriptable Methods
} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region TickableEditorTask Reflection Data

HYP_BEGIN_CLASS(TickableEditorTask, 203, 1, NAME("EditorTaskBase"), HypClassAttribute("abstract", true),HypClassAttribute("description", "A task that runs on the game thread and is has Process called every tick"))
    HypMethod(NAME(HYP_STR(IsCommitted)), &TickableEditorTask::IsCommitted),
    HypMethod(NAME(HYP_STR(Cancel)), &TickableEditorTask::Cancel, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(IsCompleted)), &TickableEditorTask::IsCompleted, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(Process)), &TickableEditorTask::Process, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(Commit)), &TickableEditorTask::Commit),
    HypMethod(NAME(HYP_STR(Tick)), &TickableEditorTask::Tick, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion TickableEditorTask Reflection Data

#pragma region TickableEditorTask Scriptable Methods

void TickableEditorTask::Cancel()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Cancel");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    Cancel_Impl();
}
bool TickableEditorTask::IsCompleted() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("IsCompleted");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr);
        }
    }

    return IsCompleted_Impl();
}
void TickableEditorTask::Process()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Process");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    Process_Impl();
}
void TickableEditorTask::Tick(float delta)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Tick");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, delta);
            return;
        }
    }

    Tick_Impl(delta);
}
#pragma endregion TickableEditorTask Scriptable Methods
} // namespace hyperion

