#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

struct RuntimeClassStub {};

static struct RuntimeScriptBindings : ScriptBindingsBase
{
    RuntimeScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<RuntimeClassStub>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        context.Global("test_generic_fn", "<T>", "function< T, T >", CxxFn<
            vm::Value, vm::Value,
            [](vm::Value value) -> vm::Value
            {
                return value;
            }
        >);

        context.Global("is_instance", "function< bool, any, Class >", CxxFn<bool, vm::Value, VMObject *,
            [](vm::Value value, VMObject *class_ptr) -> bool
            {
                if (!class_ptr) {
                    return false;
                }

                vm::VMObject *target_ptr = nullptr;
                bool is_instance = false;

                if (value.GetPointer<vm::VMObject>(&target_ptr)) {
                    // target is an object, we compare the base class of target
                    // to the value we have in class_object_ptr.

                    if (HeapValue *target_class = target_ptr->GetClassPointer()) {
                        constexpr UInt max_depth = 1024;
                        UInt depth = 0;

                        vm::VMObject *target_class_object = target_class->GetPointer<vm::VMObject>();

                        while (target_class_object != nullptr && depth < max_depth) {
                            is_instance = (*target_class_object == *class_ptr);

                            if (is_instance) {
                                break;
                            }

                            vm::Value base = vm::Value(vm::Value::NONE, { .ptr = nullptr });

                            if (!(target_class_object->LookupBasePointer(&base) && base.GetPointer<vm::VMObject>(&target_class_object))) {
                                break;
                            }

                            depth++;
                        }

                        if (depth == max_depth) {
                            // max depth reached

                            DebugLog(LogType::Warn, "Max depth reached while checking if object is instance of class\n");

                            return false;
                        }
                    }
                } else {
                    Member *proto_mem = class_ptr->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH, false);

                    if (proto_mem != nullptr) {
                        // check target's type is equal to the type of $proto value,
                        // since it is not an object pointer
                        is_instance = (value.m_type == proto_mem->value.m_type);
                    }
                }

                return is_instance;
            }
        >);
    }

} runtime_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion