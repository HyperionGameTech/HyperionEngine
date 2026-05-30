#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

class ObjectBase;
class ScriptObjectResource;
struct ScriptInstance;

namespace memory {
class Pool;
} // namespace memory

namespace dotnet {
struct ObjectReference;
class ManagedClass;
class ManagedObject;
} // namespace dotnet

namespace memory {
template <class T, class RefCountType>
class RefCountedPtr;
} // namespace memory

namespace threading {
template <class T, class Tag>
class AtomicVar;
} // namespace threading

struct CORE_API ScriptObjectFunctions
{
    static void (*IncScriptObjectRef)(ObjectBase*);
    static void (*DecScriptObjectRef)(ObjectBase*);

    static ScriptObjectResource* (*CreateScriptObjectResource_DotNet)(ObjectBase*, const memory::RefCountedPtr<dotnet::ManagedClass, threading::AtomicVar<unsigned int, void>>&);
    static ScriptObjectResource* (*CreateScriptObjectResource_Script)(ScriptInstance*, ObjectBase*);
    static void (*DestroyScriptObjectResource)(ScriptObjectResource*);

    static unsigned int (*GetScriptLanguageMask)(const ScriptObjectResource*);
    static dotnet::ManagedObject* (*GetManagedObject)(const ScriptObjectResource*);

    static memory::RefCountedPtr<dotnet::ManagedClass, threading::AtomicVar<unsigned int, void>> (*ManagedClassRefCountedPtrFromThis)(dotnet::ManagedClass*);
    static void (*ManagedClassNewManagedObject)(dotnet::ManagedClass*, void* contextPtr, void (*copyCallback)(void*, void*, unsigned int), dotnet::ObjectReference* outRef);

    static memory::Pool* (*GetScriptPool)();
};

} // namespace Hyperion
