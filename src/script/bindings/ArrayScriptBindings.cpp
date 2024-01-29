#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

static HYP_SCRIPT_FUNCTION(ArraySize)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    Int64 len = 0;

    Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    const int buffer_size = 256;
    char buffer[buffer_size];
    std::snprintf(
        buffer,
        buffer_size,
        "__array_size() is undefined for type '%s'",
        target_ptr->GetTypeString()
    );
    vm::Exception e(buffer);

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union
        {
            VMArray *array_ptr;
        } data;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.array_ptr = target_ptr->GetValue().ptr->GetPointer<VMArray>()) != nullptr) {
            // get length of array
            len = data.array_ptr->GetSize();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    HYP_SCRIPT_RETURN_INT64(len);
}

static HYP_SCRIPT_FUNCTION(ArrayPush)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 2);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("__array_push() requires an array argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::VMArray *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMArray>()) != nullptr) {
            array_ptr->PushMany(params.nargs - 1, &params.args[1]);
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return same value
    HYP_SCRIPT_RETURN(*target_ptr);
}

static HYP_SCRIPT_FUNCTION(ArrayPop)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("__array_pop() requires an array argument");

    vm::Value value; // value that was popped from array

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::VMArray *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMArray>()) != nullptr) {
            if (array_ptr->GetSize() == 0) {
                params.handler->state->ThrowException(
                    params.handler->thread,
                    vm::Exception::OutOfBoundsException()
                );

                return;
            }
            
            value = Value(array_ptr->AtIndex(array_ptr->GetSize() - 1));

            array_ptr->Pop();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return popped value
    HYP_SCRIPT_RETURN(value);
}

static HYP_SCRIPT_FUNCTION(ArrayGet)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("__array_get() requires an array argument");

    vm::Value value; // value that was popped from array

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::VMArray *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMArray>()) != nullptr) {
            vm::Number index;

            if (!params.args[1]->GetSignedOrUnsigned(&index)) {
                params.handler->state->ThrowException(
                    params.handler->thread,
                    vm::Exception::InvalidArgsException("__array_get() expects an integer as the second argument")
                );

                return;
            }

            if (index.flags & vm::Number::FLAG_SIGNED) {
                if (index.i < 0 || index.i >= array_ptr->GetSize()) {
                    params.handler->state->ThrowException(
                        params.handler->thread,
                        vm::Exception::OutOfBoundsException()
                    );

                    return;
                }
            
                value = Value(array_ptr->AtIndex(index.i));
            } else {
                if (index.u >= array_ptr->GetSize()) {
                    params.handler->state->ThrowException(
                        params.handler->thread,
                        vm::Exception::OutOfBoundsException()
                    );

                    return;
                }
                
                value = Value(array_ptr->AtIndex(index.u));
            }
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return popped value
    HYP_SCRIPT_RETURN(value);
}

static HYP_SCRIPT_FUNCTION(ArraySet)
{
    HYP_SCRIPT_CHECK_ARGS(==, 3);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("__array_set() requires an array argument");

    vm::Value value; // value that was popped from array

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::VMArray *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMArray>()) != nullptr) {
            vm::Number index;

            if (!params.args[1]->GetSignedOrUnsigned(&index)) {
                params.handler->state->ThrowException(
                    params.handler->thread,
                    vm::Exception::InvalidArgsException("__array_set() expects an integer as the second argument")
                );

                return;
            }

            if (index.flags & vm::Number::FLAG_SIGNED) {
                if (index.i < 0 || index.i >= array_ptr->GetSize()) {
                    params.handler->state->ThrowException(
                        params.handler->thread,
                        vm::Exception::OutOfBoundsException()
                    );

                    return;
                }
            
                array_ptr->AtIndex(index.i) = *params.args[2];
                array_ptr->AtIndex(index.i).Mark();
            } else {
                if (index.u >= array_ptr->GetSize()) {
                    params.handler->state->ThrowException(
                        params.handler->thread,
                        vm::Exception::OutOfBoundsException()
                    );

                    return;
                }
                
                array_ptr->AtIndex(index.u) = *params.args[2];
                array_ptr->AtIndex(index.u).Mark();
            }
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return same value
    HYP_SCRIPT_RETURN(*target_ptr);
}

static struct ArrayScriptBindings : ScriptBindingsBase
{
    ArrayScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<VMArray>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        context.Global("__array_size", "function< int, any >", ArraySize);
        context.Global("__array_push", "function< int, any, any >", ArrayPush);
        context.Global("__array_pop", "function< int, any >", ArrayPop);
        context.Global("__array_get", "function< int, any, int >", ArrayGet);
        context.Global("__array_set", "function< int, any, int, any >", ArraySet);
    }

} array_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion