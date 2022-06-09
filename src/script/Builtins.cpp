#include <script/Builtins.hpp>
#include <script/vm/InstructionHandler.hpp>

namespace hyperion {

using namespace vm;
using namespace compiler;

HYP_SCRIPT_FUNCTION(ScriptFunctions::ArraySize)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    aint64 len = 0;

    Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    const int buffer_size = 256;
    char buffer[buffer_size];
    std::snprintf(
        buffer,
        buffer_size,
        "length() is undefined for type '%s'",
        target_ptr->GetTypeString()
    );
    vm::Exception e(buffer);

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union {
            ImmutableString *str_ptr;
            Array *array_ptr;
            Object *obj_ptr;
        } data;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.str_ptr = target_ptr->GetValue().ptr->GetPointer<ImmutableString>()) != nullptr) {
            // get length of string
            len = data.str_ptr->GetLength();
        } else if ((data.array_ptr = target_ptr->GetValue().ptr->GetPointer<Array>()) != nullptr) {
            // get length of array
            len = data.array_ptr->GetSize();
        } else if ((data.obj_ptr = target_ptr->GetValue().ptr->GetPointer<Object>()) != nullptr) {
            // get number of members in object
            const TypeInfo *type_ptr = data.obj_ptr->GetTypePtr();
            AssertThrow(type_ptr != nullptr);

            len = type_ptr->GetSize();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    HYP_SCRIPT_RETURN_INT64(len);
}

void ScriptFunctions::Build(APIInstance &api_instance)
{
    api_instance.Module(hyperion::compiler::Config::global_module_name)
        .Function(
            "ArraySize",
            BuiltinTypes::INT,
            {
                { "self", BuiltinTypes::ANY } // one of: Array, String, Object
            },
            ArraySize
        );
}

} // namespace hyperion
