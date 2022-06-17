#include <script/ScriptBindings.hpp>
#include <script/vm/InstructionHandler.hpp>
#include <script/vm/MemoryBuffer.hpp>

#include <scene/node.h>

#include <math/math_util.h>
 
#include <core/lib/type_map.h>

namespace hyperion {

using namespace vm;
using namespace compiler;

APIInstance::ClassBindings ScriptBindings::class_bindings = {};
// static APIInstance::ClassBindings class_bindings = {};

HYP_SCRIPT_FUNCTION(ScriptBindings::NodeGetName)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Object *self = nullptr;

    if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
        !(self = params.args[0]->GetValue().ptr->GetPointer<vm::Object>())) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Node::GetName() expects one argument of type Node"));
        return;
    }

    vm::Member *self_member = nullptr;
    AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

    v2::Node *node_ptr;
    AssertThrow(self_member->value.GetUserData(&node_ptr));
    
    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(ImmutableString(node_ptr->GetName()));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::NodeGetLocalTranslation)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Object *self = nullptr;

    if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
        !(self = params.args[0]->GetValue().ptr->GetPointer<vm::Object>())) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Node::GetLocalTranslation() expects one argument of type Node"));
        return;
    }

    vm::Member *self_member = nullptr;
    AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

    v2::Node *node_ptr;
    AssertThrow(self_member->value.GetUserData(&node_ptr));
    
    // create heap value for Vector3
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(node_ptr->GetLocalTranslation());
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)() const>
HYP_SCRIPT_FUNCTION(CxxMemberFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);
    
    if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(0, arg0);

        HYP_SCRIPT_RETURN_INT32((arg0->*MemFn)());
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(0, arg0);

        HYP_SCRIPT_RETURN_INT64((arg0->*MemFn)());
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(0, arg0);

        HYP_SCRIPT_RETURN_UINT32((arg0->*MemFn)());
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(0, arg0);

        HYP_SCRIPT_RETURN_UINT64((arg0->*MemFn)());
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(0, arg0);

        HYP_SCRIPT_RETURN_FLOAT32((arg0->*MemFn)());
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(0, arg0);

        HYP_SCRIPT_RETURN_FLOAT64((arg0->*MemFn)());
    } else {
        HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, self_arg);

        HYP_SCRIPT_GET_MEMBER_PTR(self_arg, "__intern", ThisType, arg0);

        HYP_SCRIPT_CREATE_PTR((arg0->*MemFn)(), result);

        const auto class_name_it = ScriptBindings::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != ScriptBindings::class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = ScriptBindings::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != ScriptBindings::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

// template <class ReturnType, class ThisType, ReturnType(ThisType::*MemFn)()>
// HYP_SCRIPT_FUNCTION(CxxMemberFn)
// {
//     CxxMemberFn<ReturnType, ThisType, std::add_const_t<ReturnType(ThisType::*MemFn)()>>(params);
// }

template <class ReturnType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(const Arg0Type &) const>
HYP_SCRIPT_FUNCTION(CxxMemberFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(0, arg0);
        HYP_SCRIPT_GET_ARG_INT(1, arg1);

        HYP_SCRIPT_RETURN_INT32((arg0->*MemFn)(arg1));
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_INT(0, arg0);
        HYP_SCRIPT_GET_ARG_INT(1, arg1);

        HYP_SCRIPT_RETURN_INT64((arg0->*MemFn)(arg1));
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(0, arg0);
        HYP_SCRIPT_GET_ARG_UINT(1, arg1);

        HYP_SCRIPT_RETURN_UINT32((arg0->*MemFn)(arg1));
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_GET_ARG_UINT(0, arg0);
        HYP_SCRIPT_GET_ARG_UINT(1, arg1);

        HYP_SCRIPT_RETURN_UINT64((arg0->*MemFn)(arg1));
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(0, arg0);
        HYP_SCRIPT_GET_ARG_FLOAT(1, arg1);

        HYP_SCRIPT_RETURN_FLOAT32((arg0->*MemFn)(arg1));
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_GET_ARG_FLOAT(0, arg0);
        HYP_SCRIPT_GET_ARG_FLOAT(1, arg1);

        HYP_SCRIPT_RETURN_FLOAT64((arg0->*MemFn)(arg1));
    } else {
        HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, self_arg);
        HYP_SCRIPT_GET_ARG_PTR(1, vm::Object, other_arg);

        HYP_SCRIPT_GET_MEMBER_PTR(self_arg, "__intern", ThisType, arg0);
        HYP_SCRIPT_GET_MEMBER_PTR(other_arg, "__intern", Arg0Type, arg1);

        HYP_SCRIPT_CREATE_PTR((arg0->*MemFn)(*arg1), result);

        const auto class_name_it = ScriptBindings::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != ScriptBindings::class_bindings.class_names.End(), "Class not registered!");

        const auto prototype_it = ScriptBindings::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != ScriptBindings::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

// template <class ReturnType, class ThisType, class Arg0Type, ReturnType(ThisType::*MemFn)(const Arg0Type &)>
// HYP_SCRIPT_FUNCTION(CxxMemberFn)
// {
//     CxxMemberFn<ReturnType, ThisType, Arg0Type, std::add_const_t<ReturnType(ThisType::*MemFn)(const Arg0Type &)>>(params);
// }

template <class ReturnType, ReturnType(*Fn)()>
HYP_SCRIPT_FUNCTION(CxxFn)
{
    HYP_SCRIPT_CHECK_ARGS(==, 0);

    if constexpr (std::is_same_v<int32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT32(Fn());
    } else if constexpr (std::is_same_v<int64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_INT64(Fn());
    } else if constexpr (std::is_same_v<uint32_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT32(Fn());
    } else if constexpr (std::is_same_v<uint64_t, ReturnType>) {
        HYP_SCRIPT_RETURN_UINT64(Fn());
    } else if constexpr (std::is_same_v<float, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT32(Fn());
    } else if constexpr (std::is_same_v<double, ReturnType>) {
        HYP_SCRIPT_RETURN_FLOAT64(Fn());
    } else {
        HYP_SCRIPT_CREATE_PTR(Fn(), result);

        const auto class_name_it = ScriptBindings::class_bindings.class_names.Find<ReturnType>();
        AssertThrowMsg(class_name_it != ScriptBindings::class_bindings.class_names.End(), "Class not registered!");
    
        const auto prototype_it = ScriptBindings::class_bindings.class_prototypes.find(class_name_it->second);
        AssertThrowMsg(prototype_it != ScriptBindings::class_bindings.class_prototypes.end(), "Class not registered!");

        vm::Object result_value(prototype_it->second); // construct from prototype
        HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

        HYP_SCRIPT_CREATE_PTR(result_value, ptr);

        HYP_SCRIPT_RETURN(ptr);
    }
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Vector3Add)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);
    HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, left_object);
    HYP_SCRIPT_GET_ARG_PTR(1, vm::Object, right_object);

    HYP_SCRIPT_GET_MEMBER_PTR(left_object, "__intern", Vector3, left_vector3);
    HYP_SCRIPT_GET_MEMBER_PTR(right_object, "__intern", Vector3, right_vector3);

    HYP_SCRIPT_CREATE_PTR(*left_vector3 + *right_vector3, result);

    vm::Object result_value(left_object->GetPrototype()); // construct from prototype
    HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

    HYP_SCRIPT_CREATE_PTR(result_value, ptr);

    HYP_SCRIPT_RETURN(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Vector3Sub)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);
    HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, left_object);
    HYP_SCRIPT_GET_ARG_PTR(1, vm::Object, right_object);

    HYP_SCRIPT_GET_MEMBER_PTR(left_object, "__intern", Vector3, left_vector3);
    HYP_SCRIPT_GET_MEMBER_PTR(right_object, "__intern", Vector3, right_vector3);

    HYP_SCRIPT_CREATE_PTR(*left_vector3 - *right_vector3, result);

    vm::Object result_value(left_object->GetPrototype()); // construct from prototype
    HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

    HYP_SCRIPT_CREATE_PTR(result_value, ptr);

    HYP_SCRIPT_RETURN(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Vector3ToString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);
    HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, self);

    vm::Member *self_member = nullptr;
    AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

    Vector3 *vector3_value;
    AssertThrow(self_member->value.GetPointer<Vector3>(&vector3_value));
    
    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    char buffer[32];
    std::snprintf(
        buffer, 32,
        "[%f, %f, %f]",
        vector3_value->x,
        vector3_value->y,
        vector3_value->z
    );

    ptr->Assign(ImmutableString(buffer));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Vector3Init)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);
    HYP_SCRIPT_GET_ARG_PTR(0, vm::Object, self);

    vm::Member *self_member = nullptr;
    AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));
    
    vm::HeapValue *ptr_result = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr_result != nullptr);
    ptr_result->Assign(Vector3());
    self->LookupMemberFromHash(hash_fnv_1("__intern"))->value = vm::Value(vm::Value::HEAP_POINTER, {.ptr = ptr_result});
    ptr_result->Mark();

    HYP_SCRIPT_RETURN_PTR(params.args[0]->GetValue().ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::ArraySize)
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
        "ArraySize() is undefined for type '%s'",
        target_ptr->GetTypeString()
    );
    vm::Exception e(buffer);

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union {
            ImmutableString *str_ptr;
            Array *array_ptr;
            MemoryBuffer *memory_buffer_ptr;
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
        } else if ((data.memory_buffer_ptr = target_ptr->GetValue().ptr->GetPointer<MemoryBuffer>()) != nullptr) {
            // get length of memory buffer
            len = data.memory_buffer_ptr->GetSize();
        } else if ((data.obj_ptr = target_ptr->GetValue().ptr->GetPointer<Object>()) != nullptr) {
            // get number of members in object
            len = data.obj_ptr->GetSize();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    HYP_SCRIPT_RETURN_INT64(len);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::ArrayPush)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 2);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("ArrayPush() requires an array argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::Array *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::Array>()) != nullptr) {
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

HYP_SCRIPT_FUNCTION(ScriptBindings::ArrayPop)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("ArrayPop() requires an array argument");

    vm::Value value; // value that was popped from array

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::Array *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::Array>()) != nullptr) {
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

HYP_SCRIPT_FUNCTION(ScriptBindings::Puts)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::ExecutionThread *thread = params.handler->thread;
    vm::VMState *state = params.handler->state;

    const auto *string_arg = params.args[0]->GetValue().ptr->GetPointer<ImmutableString>();

    if (!string_arg) {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException("string")
        );

        return;
    }

    int puts_result = std::puts(string_arg->GetData());

    HYP_SCRIPT_RETURN_INT32(puts_result);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::ToString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);

    AssertThrow(ptr != nullptr);
    ptr->Assign(params.args[0]->ToString());

    vm::Value res;
    // assign register value to the allocated object
    res.m_type = vm::Value::HEAP_POINTER;
    res.m_value.ptr = ptr;

    ptr->Mark();

    HYP_SCRIPT_RETURN(res);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Format)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Format() expects a string as the first argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(params.handler->thread, vm::Exception::NullReferenceException());
        } else if (vm::ImmutableString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::ImmutableString>()) {
            // scan through string and merge each argument where there is a '%'
            const size_t original_length = str_ptr->GetLength();

            std::string result_string;
            result_string.reserve(original_length);

            const char *original_data = str_ptr->GetData();
            AssertThrow(original_data != nullptr);

            const int buffer_size = 256;
            char buffer[buffer_size + 1] = {0};

            // number of '%' characters handled
            int num_fmts = 1;
            int buffer_idx = 0;

            for (size_t i = 0; i < original_length; i++) {
                if (original_data[i] == '%' && num_fmts < params.nargs) {
                    // set end of buffer to be NUL
                    buffer[buffer_idx + 1] = '\0';
                    // now upload to result string
                    result_string += buffer;
                    // clear buffer
                    buffer_idx = 0;
                    buffer[0] = '\0';

                    vm::ImmutableString str = params.args[num_fmts++]->ToString();

                    result_string.append(str.GetData());
                } else {
                    buffer[buffer_idx] = original_data[i];

                    if (buffer_idx == buffer_size - 1 || i == original_length - 1) {
                        // set end of buffer to be NUL
                        buffer[buffer_idx + 1] = '\0';
                        // now upload to result string
                        result_string += buffer;
                        //clear buffer
                        buffer_idx = 0;
                        buffer[0] = '\0';
                    } else {
                        buffer_idx++;
                    }
                }
            }

            while (num_fmts < params.nargs) {
                vm::ImmutableString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            // store the result in a variable
            vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
            AssertThrow(ptr != nullptr);
            // assign it to the formatted string
            ptr->Assign(vm::ImmutableString(result_string.data()));

            vm::Value res;
            // assign register value to the allocated object
            res.m_type = vm::Value::HEAP_POINTER;
            res.m_value.ptr = ptr;

            ptr->Mark();

            HYP_SCRIPT_RETURN(res);
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Print)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Print() expects a string as the first argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(params.handler->thread, vm::Exception::NullReferenceException());
        } else if (vm::ImmutableString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::ImmutableString>()) {
            // scan through string and merge each argument where there is a '%'
            const size_t original_length = str_ptr->GetLength();

            std::string result_string;
            result_string.reserve(original_length);

            const char *original_data = str_ptr->GetData();
            AssertThrow(original_data != nullptr);

            const int buffer_size = 256;
            char buffer[buffer_size + 1] = {0};

            // number of '%' characters handled
            int num_fmts = 1;
            int buffer_idx = 0;

            for (size_t i = 0; i < original_length; i++) {
                if (original_data[i] == '%' && num_fmts < params.nargs) {
                    // set end of buffer to be NUL
                    buffer[buffer_idx + 1] = '\0';
                    // now upload to result string
                    result_string += buffer;
                    // clear buffer
                    buffer_idx = 0;
                    buffer[0] = '\0';

                    vm::ImmutableString str = params.args[num_fmts++]->ToString();

                    result_string.append(str.GetData());
                } else {
                    buffer[buffer_idx] = original_data[i];

                    if (buffer_idx == buffer_size - 1 || i == original_length - 1) {
                        // set end of buffer to be NUL
                        buffer[buffer_idx + 1] = '\0';
                        // now upload to result string
                        result_string += buffer;
                        //clear buffer
                        buffer_idx = 0;
                        buffer[0] = '\0';
                    } else {
                        buffer_idx++;
                    }
                }
            }

            while (num_fmts < params.nargs) {
                vm::ImmutableString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            utf::cout << result_string;
            
            HYP_SCRIPT_RETURN_INT32(result_string.size());
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Malloc)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Malloc() expects an integer as the first argument");

    Number num;

    if (target_ptr->GetSignedOrUnsigned(&num)) {
        // create heap value for string
        vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);

        auint64 malloc_size = num.flags & Number::FLAG_SIGNED
            ? static_cast<auint64>(MathUtil::Max(0, num.i))
            : num.u;

        AssertThrow(ptr != nullptr);
        ptr->Assign(vm::MemoryBuffer(malloc_size));

        vm::Value res;
        // assign register value to the allocated object
        res.m_type = vm::Value::HEAP_POINTER;
        res.m_value.ptr = ptr;

        ptr->Mark();

        HYP_SCRIPT_RETURN(res);
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptBindings::Free)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Free() expects a pointer type");

    if (target_ptr->GetType() == Value::HEAP_POINTER) {
        // just mark nullptr, gc will collect it.
        target_ptr->GetValue().ptr = nullptr;
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

void ScriptBindings::Build(APIInstance &api_instance)
{
    // auto vector3_add = CxxMemberFn<Vector3, Vector3, Vector3, &Vector3::operator+>;
    // static_assert(std::is_same_v<decltype(vector3_add), NativeFunctionPtr_t>);

    api_instance.Module(hyperion::compiler::Config::global_module_name)
        .Variable("SCRIPT_VERSION", 200)
        .Variable("ENGINE_VERSION", 200)
        .Class<Vector3>(
            "Vector3",
            {
                { "__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, {.ptr = nullptr}) },
                {
                    "operator+",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, Vector3, &Vector3::operator+ >
                },
                {
                    "operator-",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, Vector3, &Vector3::operator- >
                },
                {
                    "ToString",
                    BuiltinTypes::STRING,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    Vector3ToString
                },
                {
                    "$construct",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxFn<Vector3, &Vector3::Zero>
                },
                {
                    "x",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    [](sdk::Params params)
                    {
                        HYP_SCRIPT_CHECK_ARGS(==, 1);

                        vm::Object *self = nullptr;

                        if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
                            !(self = params.args[0]->GetValue().ptr->GetPointer<vm::Object>())) {
                            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Vector3::x() expects one argument of type Vector3"));
                            return;
                        }

                        vm::Member *self_member = nullptr;
                        AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

                        Vector3 *self_vector3 = nullptr;
                        AssertThrow(self_member->value.GetPointer(&self_vector3));
                        
                        HYP_SCRIPT_RETURN_FLOAT32(self_vector3->x);
                    }
                },
                {
                    "y",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    [](sdk::Params params)
                    {
                        HYP_SCRIPT_CHECK_ARGS(==, 1);

                        vm::Object *self = nullptr;

                        if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
                            !(self = params.args[0]->GetValue().ptr->GetPointer<vm::Object>())) {
                            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Vector3::y() expects one argument of type Vector3"));
                            return;
                        }

                        vm::Member *self_member = nullptr;
                        AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

                        Vector3 *self_vector3 = nullptr;
                        AssertThrow(self_member->value.GetPointer(&self_vector3));
                        
                        HYP_SCRIPT_RETURN_FLOAT32(self_vector3->y);
                    }
                },
                {
                    "z",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    [](sdk::Params params)
                    {
                        HYP_SCRIPT_CHECK_ARGS(==, 1);

                        vm::Object *self = nullptr;

                        if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
                            !(self = params.args[0]->GetValue().ptr->GetPointer<vm::Object>())) {
                            params.handler->state->ThrowException(params.handler->thread, vm::Exception("Vector3::z() expects one argument of type Vector3"));
                            return;
                        }

                        vm::Member *self_member = nullptr;
                        AssertThrow(self_member = self->LookupMemberFromHash(hash_fnv_1("__intern")));

                        Vector3 *self_vector3 = nullptr;
                        AssertThrow(self_member->value.GetPointer(&self_vector3));
                        
                        HYP_SCRIPT_RETURN_FLOAT32(self_vector3->z);
                    }
                }
            }
        )
        .Function(
            "ArraySize",
            BuiltinTypes::INT,
            {
                { "self", BuiltinTypes::ANY } // one of: Array, String, Object
            },
            ArraySize
        )
        .Function(
            "ArrayPush",
            BuiltinTypes::ARRAY,
            {
                { "self", BuiltinTypes::ARRAY },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            ArrayPush
        )
        .Function(
            "ArrayPop",
            BuiltinTypes::ANY, // returns object that was popped
            {
                { "self", BuiltinTypes::ARRAY }
            },
            ArrayPop
        )
        .Function(
            "Puts",
            BuiltinTypes::INT,
            {
                { "str", BuiltinTypes::STRING }
            },
            Puts
        )
        .Function(
            "ToString",
            BuiltinTypes::STRING,
            {
                { "obj", BuiltinTypes::ANY }
            },
            ToString
        )
        .Function(
            "Format",
            BuiltinTypes::STRING,
            {
                { "format", BuiltinTypes::STRING },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            Format
        )
        .Function(
            "Print",
            BuiltinTypes::INT,
            {
                { "format", BuiltinTypes::STRING },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            Print
        )
        .Function(
            "Malloc",
            BuiltinTypes::ANY,
            {
                { "size", BuiltinTypes::INT } // TODO: should be unsigned, but having conversion issues
            },
            Malloc
        )
        .Function(
            "Free",
            BuiltinTypes::VOID,
            {
                { "ptr", BuiltinTypes::ANY } // TODO: should be unsigned, but having conversion issues
            },
            Free
        );

    class_bindings = api_instance.class_bindings;
}

} // namespace hyperion
