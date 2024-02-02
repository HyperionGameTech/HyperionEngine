#include <Engine.hpp>
#include <Types.hpp>

#include <core/lib/Mutex.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/String.hpp>

#include <dotnet_support/ClassObject.hpp>
#include <dotnet_support/Assembly.hpp>

using namespace hyperion;
using namespace hyperion::v2;
using namespace hyperion::dotnet;

extern "C" {
    void NativeInterop_Initialize(void *invoke_method_fptr)
    {
        // g_invoke_method_fptr = reinterpret_cast<void *(*)(ManagedMethod *, void **)>(invoke_method_fptr);

        // // test
        // ClassObject *logger_class_object = s_class_object_holder.FindClassByName("Logger");

        // if (logger_class_object) {
        //     void *return_value = logger_class_object->InvokeMethod<void *, Int32, char *>("TestMethod", 9, "hello hello world!!!");
        // }
    }

    void NativeInterop_SetInvokeMethodFunction(ClassObjectHolder *class_holder, void *invoke_method_fptr)
    {
        AssertThrow(class_holder != nullptr);

        class_holder->SetInvokeMethodFunction(reinterpret_cast<ClassObjectHolder::InvokeMethodFunction>(invoke_method_fptr));
    }

    ManagedClass ManagedClass_Create(ClassObjectHolder *class_holder, Int32 type_hash, const char *type_name)
    {
        AssertThrow(class_holder != nullptr);

        DebugLog(LogType::Debug, "(C++) Creating managed class: %s\t%p\n", type_name, class_holder);

        ClassObject *class_object = class_holder->GetOrCreateClassObject(type_hash, type_name);

        return ManagedClass { type_hash, class_object };
    }

    void ManagedClass_AddMethod(ManagedClass managed_class, const char *method_name, void *method_info_ptr)
    {
        DebugLog(LogType::Debug, "(C++) Adding method...\n");

        if (!managed_class.class_object || !method_name || !method_info_ptr) {
            return;
        }

        DebugLog(LogType::Debug, "(C++) Adding method: %s to class: %s\n", method_name, managed_class.class_object->GetName().Data());

        ManagedMethod method_object;
        method_object.method_info_ptr = method_info_ptr;

        managed_class.class_object->AddMethod(method_name, std::move(method_object));
    }

    // const char *ManagedClass_GetName(ManagedClass *managed_class)
    // {
    //     if (!managed_class || !managed_class->class_object) {
    //         return nullptr;
    //     }

    //     return managed_class->class_object->name.Data();
    // }
}