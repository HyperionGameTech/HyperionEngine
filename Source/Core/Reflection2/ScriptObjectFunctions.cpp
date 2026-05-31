#include <Core/reflection/ScriptObjectFunctions.hpp>

namespace Hyperion {

void (*ScriptObjectFunctions::IncScriptObjectRef)(ObjectBase*) = nullptr;
void (*ScriptObjectFunctions::DecScriptObjectRef)(ObjectBase*) = nullptr;

ScriptObjectResource* (*ScriptObjectFunctions::CreateScriptObjectResource_DotNet)(ObjectBase*, const memory::RefCountedPtr<dotnet::ManagedClass, threading::AtomicVar<unsigned int, void>>&) = nullptr;
ScriptObjectResource* (*ScriptObjectFunctions::CreateScriptObjectResource_Script)(ScriptInstance*, ObjectBase*) = nullptr;
void (*ScriptObjectFunctions::DestroyScriptObjectResource)(ScriptObjectResource*) = nullptr;

unsigned int (*ScriptObjectFunctions::GetScriptLanguageMask)(const ScriptObjectResource*) = nullptr;
dotnet::ManagedObject* (*ScriptObjectFunctions::GetManagedObject)(const ScriptObjectResource*) = nullptr;

memory::RefCountedPtr<dotnet::ManagedClass, threading::AtomicVar<unsigned int, void>> (*ScriptObjectFunctions::ManagedClassRefCountedPtrFromThis)(dotnet::ManagedClass*) = nullptr;
void (*ScriptObjectFunctions::ManagedClassNewManagedObject)(dotnet::ManagedClass*, void* contextPtr, void (*copyCallback)(void*, void*, unsigned int), dotnet::ObjectReference* outRef) = nullptr;

memory::Pool* (*ScriptObjectFunctions::GetScriptPool)() = nullptr;

} // namespace Hyperion
