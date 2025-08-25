#ifdef HYP_SCRIPT

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

struct RuntimeClassStub
{
};

static struct RuntimeScriptBindings : ScriptBindingsBase
{
    RuntimeScriptBindings()
        : ScriptBindingsBase(TypeId::ForType<RuntimeClassStub>())
    {
    }

    virtual void Generate(scriptapi2::Context& context) override
    {
#if 0
        context.Global("testGenericFn", "<T>", "function< T, T >", CxxFn<vm::Value, vm::Value, [](vm::Value value) -> vm::Value
                                                                       {
                                                                           return value;
                                                                       }>);

        context.Global("isInstance", "function< bool, any, Class >", CxxFn<bool, vm::Value, VMObject*, [](vm::Value value, VMObject* classPtr) -> bool
                                                                         {
                                                                             if (!classPtr)
                                                                             {
                                                                                 return false;
                                                                             }

                                                                             vm::VMObject* targetPtr = nullptr;
                                                                             bool isInstance = false;

                                                                             if (value.GetPointer<vm::VMObject>(&targetPtr))
                                                                             {
                                                                                 // target is an object, we compare the base class of target
                                                                                 // to the value we have in classObjectPtr.

                                                                                 if (HeapValue* targetClass = targetPtr->GetClassPointer())
                                                                                 {
                                                                                     constexpr uint32 maxDepth = 1024;
                                                                                     uint32 depth = 0;

                                                                                     vm::VMObject* targetClassObject = targetClass->GetPointer<vm::VMObject>();

                                                                                     while (targetClassObject != nullptr && depth < maxDepth)
                                                                                     {
                                                                                         isInstance = (*targetClassObject == *classPtr);

                                                                                         if (isInstance)
                                                                                         {
                                                                                             break;
                                                                                         }

                                                                                         vm::Value base = vm::Value(vm::Value::NONE, { .internal = { .ptr = nullptr } });

                                                                                         if (!(targetClassObject->LookupBasePointer(&base) && base.GetPointer<vm::VMObject>(&targetClassObject)))
                                                                                         {
                                                                                             break;
                                                                                         }

                                                                                         depth++;
                                                                                     }

                                                                                     if (depth == maxDepth)
                                                                                     {
                                                                                         // max depth reached

                                                                                         DebugLog(LogType::Warn, "Max depth reached while checking if object is instance of class\n");

                                                                                         return false;
                                                                                     }
                                                                                 }
                                                                             }
                                                                             else
                                                                             {
                                                                                 Member* protoMem = classPtr->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH, false);

                                                                                 if (protoMem != nullptr)
                                                                                 {
                                                                                     // check target's type is equal to the type of $proto value,
                                                                                     // since it is not an object pointer
                                                                                     isInstance = (value.m_type == protoMem->value.m_type);
                                                                                 }
                                                                             }

                                                                             return isInstance;
                                                                         }>);
#endif
    }

} runtimeScriptBindings {};

} // namespace bindings
} // namespace script
} // namespace hyperion

#endif