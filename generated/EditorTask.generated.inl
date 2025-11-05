#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EditorTaskBase Reflection Data

HYP_BEGIN_CLASS(EditorTaskBase, 202, 3, NAME("HypObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(IsCommitted)), &EditorTaskBase::IsCommitted),
    Method(NAME(HYP_STR(Cancel)), &EditorTaskBase::Cancel),
    Method(NAME(HYP_STR(IsCompleted)), &EditorTaskBase::IsCompleted),
    Method(NAME(HYP_STR(Process)), &EditorTaskBase::Process),
    Method(NAME(HYP_STR(Commit)), &EditorTaskBase::Commit),
    Field(NAME(HYP_STR(OnComplete)), &EditorTaskBase::OnComplete, offsetof(EditorTaskBase, OnComplete), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } }),
    Field(NAME(HYP_STR(OnCancel)), &EditorTaskBase::OnCancel, offsetof(EditorTaskBase, OnCancel), Span<const ClassAttribute> { {ClassAttribute("scriptabledelegate", true) } })
HYP_END_CLASS

#pragma endregion EditorTaskBase Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region LongRunningEditorTask Reflection Data

HYP_BEGIN_CLASS(LongRunningEditorTask, 203, 0, NAME("EditorTaskBase"), ClassAttribute("abstract", true),ClassAttribute("description", "A task that runs on a Task thread and has Process called one time only"))
    Method(NAME(HYP_STR(IsCommitted)), &LongRunningEditorTask::IsCommitted),
    Method(NAME(HYP_STR(Cancel)), &LongRunningEditorTask::Cancel, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(IsCompleted)), &LongRunningEditorTask::IsCompleted, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Process)), &LongRunningEditorTask::Process, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Commit)), &LongRunningEditorTask::Commit)
HYP_END_CLASS

#pragma endregion LongRunningEditorTask Reflection Data

#pragma region LongRunningEditorTask Scriptable Methods

void LongRunningEditorTask::Cancel()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Cancel");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region TickableEditorTask Reflection Data

HYP_BEGIN_CLASS(TickableEditorTask, 204, 1, NAME("EditorTaskBase"), ClassAttribute("abstract", true),ClassAttribute("description", "A task that runs on the game thread and is has Process called every tick"))
    Method(NAME(HYP_STR(IsCommitted)), &TickableEditorTask::IsCommitted),
    Method(NAME(HYP_STR(Cancel)), &TickableEditorTask::Cancel, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(IsCompleted)), &TickableEditorTask::IsCompleted, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Process)), &TickableEditorTask::Process, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Commit)), &TickableEditorTask::Commit),
    Method(NAME(HYP_STR(Tick)), &TickableEditorTask::Tick, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion TickableEditorTask Reflection Data

#pragma region TickableEditorTask Scriptable Methods

void TickableEditorTask::Cancel()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Cancel");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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

