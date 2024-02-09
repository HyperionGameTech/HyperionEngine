#include <Engine.hpp>
#include <Types.hpp>

#include <core/lib/Mutex.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/String.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/interop/ManagedMethod.hpp>
#include <dotnet/interop/ManagedGuid.hpp>

using namespace hyperion;
using namespace hyperion::v2;
using namespace hyperion::dotnet;

extern "C" {
    void NativeInterop_Initialize(void *invoke_method_fptr)
    {
        // g_invoke_method_fptr = reinterpret_cast<void *(*)(ManagedMethod *, void **)>(invoke_method_fptr);

        // // test
        // Class *logger_class_object = s_class_object_holder.FindClassByName("Logger");

        // if (logger_class_object) {
        //     void *return_value = logger_class_object->InvokeMethod<void *, int32, char *>("TestMethod", 9, "hello hello world!!!");
        // }
    }

    void NativeInterop_SetInvokeMethodFunction(ClassHolder *class_holder, ClassHolder::InvokeMethodFunction invoke_method_fptr)
    {
        AssertThrow(class_holder != nullptr);

        class_holder->SetInvokeMethodFunction(invoke_method_fptr);
    }

    ManagedClass ManagedClass_Create(ClassHolder *class_holder, int32 type_hash, const char *type_name)
    {
        AssertThrow(class_holder != nullptr);

        DebugLog(LogType::Debug, "(C++) Creating managed class: %s\t%p\n", type_name, class_holder);

        Class *class_object = class_holder->GetOrCreateClassObject(type_hash, type_name);

        return ManagedClass { type_hash, class_object };
    }

    void ManagedClass_AddMethod(ManagedClass managed_class, const char *method_name, ManagedGuid guid)
    {
        DebugLog(LogType::Debug, "(C++) Adding method...\n");

        if (!managed_class.class_object || !method_name) {
            return;
        }

        DebugLog(LogType::Debug, "(C++) Adding method: %s to class: %s\n", method_name, managed_class.class_object->GetName().Data());

        ManagedMethod method_object;
        method_object.guid = guid;

        managed_class.class_object->AddMethod(method_name, std::move(method_object));
    }

    void ManagedClass_SetNewObjectFunction(ManagedClass managed_class, Class::NewObjectFunction new_object_fptr)
    {
        if (!managed_class.class_object) {
            return;
        }

        managed_class.class_object->SetNewObjectFunction(new_object_fptr);
    }

    void ManagedClass_SetFreeObjectFunction(ManagedClass managed_class, Class::FreeObjectFunction free_object_fptr)
    {
        if (!managed_class.class_object) {
            return;
        }

        managed_class.class_object->SetFreeObjectFunction(free_object_fptr);
    }

    // const char *ManagedClass_GetName(ManagedClass *managed_class)
    // {
    //     if (!managed_class || !managed_class->class_object) {
    //         return nullptr;
    //     }

    //     return managed_class->class_object->name.Data();
    // }
}