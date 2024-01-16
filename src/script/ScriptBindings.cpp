#include <script/ScriptBindings.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/VMMemoryBuffer.hpp>

#include <asset/BufferedByteReader.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstString.hpp>

#include <scene/Node.hpp>
#include <scene/Entity.hpp>
#include <Engine.hpp>

#include <math/MathUtil.hpp>
 
#include <core/lib/TypeMap.hpp>

#include "compiler/dis/DecompilationUnit.hpp"

namespace hyperion {

using namespace vm;
using namespace compiler;

struct FilePointerMap
{
    HashMap<UInt32, FILE *> data;
    UInt32 counter = 0;
};

thread_local FilePointerMap file_pointer_map;

APIInstance::ClassBindings ScriptBindings::class_bindings = {};
// static APIInstance::ClassBindings class_bindings = {};

HYP_SCRIPT_FUNCTION(ScriptBindings::NodeGetName)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::VMObject *self = nullptr;

    if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
        !(self = params.args[0]->GetValue().ptr->GetPointer<vm::VMObject>())) {
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

    ptr->Assign(VMString(node_ptr->GetName().Data()));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::NodeGetLocalTranslation)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::VMObject *self = nullptr;

    if (params.args[0]->GetType() != vm::Value::HEAP_POINTER ||
        !(self = params.args[0]->GetValue().ptr->GetPointer<vm::VMObject>())) {
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


HYP_SCRIPT_FUNCTION(ScriptBindings::Vector3ToString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    Vector3 vector3_value = GetArgument<0, Vector3>(params);
    
    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    char buffer[32];
    std::snprintf(
        buffer, 32,
        "[%f, %f, %f]",
        vector3_value.x,
        vector3_value.y,
        vector3_value.z
    );

    ptr->Assign(VMString(buffer));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

HYP_SCRIPT_FUNCTION(ScriptBindings::ArraySize)
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
        "ArraySize() is undefined for type '%s'",
        target_ptr->GetTypeString()
    );
    vm::Exception e(buffer);

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union
        {
            VMString *str_ptr;
            VMArray *array_ptr;
            VMMemoryBuffer *memory_buffer_ptr;
            VMObject *obj_ptr;
        } data;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.str_ptr = target_ptr->GetValue().ptr->GetPointer<VMString>()) != nullptr) {
            // get length of string
            len = data.str_ptr->GetLength();
        } else if ((data.array_ptr = target_ptr->GetValue().ptr->GetPointer<VMArray>()) != nullptr) {
            // get length of array
            len = data.array_ptr->GetSize();
        } else if ((data.memory_buffer_ptr = target_ptr->GetValue().ptr->GetPointer<VMMemoryBuffer>()) != nullptr) {
            // get length of memory buffer
            len = data.memory_buffer_ptr->GetSize();
        } else if ((data.obj_ptr = target_ptr->GetValue().ptr->GetPointer<VMObject>()) != nullptr) {
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

HYP_SCRIPT_FUNCTION(ScriptBindings::ArrayPop)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("ArrayPop() requires an array argument");

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

HYP_SCRIPT_FUNCTION(ScriptBindings::Puts)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::ExecutionThread *thread = params.handler->thread;
    vm::VMState *state = params.handler->state;

    const auto *string_arg = params.args[0]->GetValue().ptr->GetPointer<VMString>();

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
        } else if (vm::VMString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMString>()) {
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

                    vm::VMString str = params.args[num_fmts++]->ToString();

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
                vm::VMString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            // store the result in a variable
            vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
            AssertThrow(ptr != nullptr);
            // assign it to the formatted string
            ptr->Assign(vm::VMString(result_string.data()));

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
        } else if (vm::VMString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::VMString>()) {
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

                    vm::VMString str = params.args[num_fmts++]->ToString();

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
                vm::VMString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            std::printf("%s", result_string.c_str());
            
            HYP_SCRIPT_RETURN_INT32(result_string.length());
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

        UInt64 malloc_size = num.flags & Number::FLAG_SIGNED
            ? static_cast<UInt64>(MathUtil::Max(0, num.i))
            : num.u;

        AssertThrow(ptr != nullptr);
        ptr->Assign(vm::VMMemoryBuffer(malloc_size));

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


static HYP_SCRIPT_FUNCTION(VM_ReadStackVar)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    const UInt32 index = GetArgument<0, UInt32>(params);

    const auto &stk = params.handler->thread->m_stack;

    if (index >= stk.GetStackPointer()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Stack index out of bounds"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    HYP_SCRIPT_RETURN(stk.GetData()[index]);
}

static HYP_SCRIPT_FUNCTION(Runtime_MakeStruct)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::VMArray *arguments_ptr = GetArgument<0, vm::VMArray *>(params);

    if (!arguments_ptr) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    VMStructDefinition struct_definition;
    struct_definition.members.Resize(arguments_ptr->GetSize());

    for (Int32 index = 0; index < arguments_ptr->GetSize(); index++) {
        Pair<String, Value> &member = struct_definition.members[index];

        vm::VMArray *item_array = nullptr;
        vm::VMString *item_type_str = nullptr;
        vm::VMString *item_name_str = nullptr;
        vm::VMString *value_str = nullptr;
        vm::VMStruct *value_struct = nullptr;
        vm::Value default_value = vm::Value(vm::Value::ValueType::NONE, { .ptr = nullptr });

        vm::VMStructType member_type = vm::VM_STRUCT_TYPE_NONE;

        vm::Value::ValueData data;
        
        if (!arguments_ptr->AtIndex(index).GetPointer<vm::VMArray>(&item_array)) {
            goto invalid_array;
        }

        if (item_array->GetSize() != 3) {
            goto invalid_array;
        }

        if (!item_array->AtIndex(0).GetPointer<vm::VMString>(&item_type_str)) {
            goto invalid_array;
        }

        if (!item_array->AtIndex(1).GetPointer<vm::VMString>(&item_name_str)) {
            goto invalid_array;
        }

        member.first = item_name_str->GetString();
        default_value = item_array->AtIndex(2);

        // Lookup a type based on the name
        if ((*item_type_str) == "i8") {
            member_type = vm::VM_STRUCT_TYPE_I8;

            if (!default_value.GetInteger(&data.i64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::I8, { .i8 = static_cast<Int8>(data.i64) });
        } else if ((*item_type_str) == "u8") {
            member_type = vm::VM_STRUCT_TYPE_U8;

            if (!default_value.GetUnsigned(&data.u64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::U8, { .u8 = static_cast<UInt8>(data.u64) });
        } else if ((*item_type_str) == "i16") {
            member_type = vm::VM_STRUCT_TYPE_I16;

            if (!default_value.GetInteger(&data.i64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::I16, { .i16 = static_cast<Int16>(data.i64) });
        } else if ((*item_type_str) == "u16") {
            member_type = vm::VM_STRUCT_TYPE_U16;

            if (!default_value.GetUnsigned(&data.u64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::U16, { .u16 = static_cast<UInt16>(data.u64) });
        } else if ((*item_type_str) == "i32") {
            member_type = vm::VM_STRUCT_TYPE_I32;

            if (!default_value.GetInteger(&data.i64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::I32, { .i32 = static_cast<Int32>(data.i64) });
        } else if ((*item_type_str) == "u32") {
            member_type = vm::VM_STRUCT_TYPE_U32;

            if (!default_value.GetUnsigned(&data.u64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::U32, { .u32 = static_cast<UInt32>(data.u64) });
        } else if ((*item_type_str) == "i64") {
            member_type = vm::VM_STRUCT_TYPE_I64;

            if (!default_value.GetInteger(&data.i64)) {
                goto invalid_member_type_given;
            }

            member.second = Value(Value::ValueType::I64, { .i64 = static_cast<Int32>(data.i64) });
        } else if ((*item_type_str) == "u64") {
            member_type = vm::VM_STRUCT_TYPE_U64;

            if (!default_value.GetUnsigned(&data.u64)) {
                goto invalid_member_type_given;    
            }

            member.second = Value(Value::ValueType::U64, { .u64 = static_cast<UInt32>(data.u64) });
        } else if ((*item_type_str) == "f32") {
            member_type = vm::VM_STRUCT_TYPE_F32;

            if (!default_value.GetFloatingPointCoerce(&data.d)) {
                goto invalid_member_type_given;  
            }

            member.second = Value(Value::ValueType::F32, { .f = static_cast<Float32>(data.d) });
        } else if ((*item_type_str) == "f64") {
            member_type = vm::VM_STRUCT_TYPE_F64;

            if (!default_value.GetFloatingPointCoerce(&data.d)) {
                goto invalid_member_type_given;  
            }

            member.second = Value(Value::ValueType::F64, { .d = static_cast<Float64>(data.d) });
        } else if ((*item_type_str) == "object" || (*item_type_str) == "string") {
            member_type = vm::VM_STRUCT_TYPE_DYNAMIC;

            member.second = default_value;
        } else {
            goto invalid_member_type_given;
        }

        continue;

invalid_member_type_given:
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid member type given"));

        HYP_SCRIPT_RETURN_VOID(nullptr);

invalid_array:
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("All arguments to MakeStruct must be an array of 2-3 elements, in format: (type: String, name: String, default_value: Any)"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }
    
    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(VMStruct::MakeStruct(struct_definition));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_GetStructMember)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::VMStruct *struct_ptr = GetArgument<0, vm::VMStruct *>(params);
    vm::VMString *member_name_ptr = GetArgument<1, vm::VMString *>(params);

    if (!struct_ptr || !member_name_ptr) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }

    HYP_SCRIPT_RETURN(struct_ptr->ReadMember(member_name_ptr->GetData()));
}

static HYP_SCRIPT_FUNCTION(Runtime_SetStructMember)
{
    HYP_SCRIPT_CHECK_ARGS(==, 3);

    vm::VMStruct *struct_ptr = GetArgument<0, vm::VMStruct *>(params);
    vm::VMString *member_name_ptr = GetArgument<1, vm::VMString *>(params);
    vm::Value *value_ptr = params.args[2];

    if (!struct_ptr || !member_name_ptr) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }


    HYP_SCRIPT_RETURN_BOOLEAN(struct_ptr->WriteMember(member_name_ptr->GetData(), value_ptr != nullptr ? *value_ptr : Value(Value::ValueType::HEAP_POINTER, { .ptr = nullptr })));
}

static HYP_SCRIPT_FUNCTION(Runtime_GetStructMemoryBuffer)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::VMStruct *struct_ptr = GetArgument<0, vm::VMStruct *>(params);

    if (!struct_ptr) {
        HYP_SCRIPT_RETURN_NULL();
    }
    
    // create heap value for struct memory buffer
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);

    AssertThrow(ptr != nullptr);
    ptr->Assign(vm::VMMemoryBuffer(struct_ptr->GetMemory()));

    vm::Value res;
    // assign register value to the allocated object
    res.m_type = vm::Value::HEAP_POINTER;
    res.m_value.ptr = ptr;

    ptr->Mark();

    HYP_SCRIPT_RETURN(res);
}

static HYP_SCRIPT_FUNCTION(Runtime_HasMember)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::Value *arg0 = params.args[0];
    VMString *str = GetArgument<1, VMString *>(params);

    if (!arg0 || !str) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *object_ptr;

    if (arg0->GetPointer<vm::VMObject>(&object_ptr)) {
        const HashFNV1 hash = hash_fnv_1(str->GetData());
        
        HYP_SCRIPT_RETURN_BOOLEAN(object_ptr->LookupMemberFromHash(hash) != nullptr);
    } else {
        HYP_SCRIPT_THROW(vm::Exception("Not an object"));
    }
}

static HYP_SCRIPT_FUNCTION(Runtime_GetMember)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::Value *arg0 = params.args[0];
    VMString *str = GetArgument<1, VMString *>(params);

    if (!arg0 || !str) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *object_ptr;

    if (arg0->GetPointer<vm::VMObject>(&object_ptr)) {
        const HashFNV1 hash = hash_fnv_1(str->GetData());
        
        Member *member = object_ptr->LookupMemberFromHash(hash);

        if (member == nullptr) {
            HYP_SCRIPT_RETURN_NULL();
        } else {
            HYP_SCRIPT_RETURN(member->value);
        }
    } else {
        HYP_SCRIPT_THROW(vm::Exception("Not an object"));
    }
}

static HYP_SCRIPT_FUNCTION(Runtime_GetMembers)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *arg0 = params.args[0];

    if (!arg0) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *object_ptr;

    if (arg0->GetPointer<vm::VMObject>(&object_ptr)) {
        vm::VMArray ary(object_ptr->GetSize());

        for (SizeType index = 0; index < object_ptr->GetSize(); index++) {
            HYP_SCRIPT_CREATE_PTR(vm::VMString(object_ptr->GetMember(index).name), member_name_value);

            ary.AtIndex(index) = member_name_value;
        }

        HYP_SCRIPT_CREATE_PTR(ary, ptr);
        HYP_SCRIPT_RETURN(ptr);
    } else {
        HYP_SCRIPT_THROW(vm::Exception("Not an object"));
    }
}

static HYP_SCRIPT_FUNCTION(Runtime_SetMember)
{
    HYP_SCRIPT_CHECK_ARGS(==, 3);

    vm::Value *arg0 = params.args[0];
    VMString *member_name_str_ptr = GetArgument<1, VMString *>(params);
    vm::Value *member_value_ptr = params.args[2];

    if (!arg0 || !member_name_str_ptr) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *object_ptr;

    if (arg0->GetPointer<vm::VMObject>(&object_ptr)) {
        if (member_value_ptr) {
            object_ptr->SetMember(
                member_name_str_ptr->GetData(),
                *member_value_ptr
            );
        } else {
            object_ptr->SetMember(
                member_name_str_ptr->GetData(),
                vm::Value(vm::Value::ValueType::HEAP_POINTER, { .ptr = nullptr })
            );
        }
    } else {
        HYP_SCRIPT_THROW(vm::Exception("Not an object"));
    }
}

static HYP_SCRIPT_FUNCTION(Runtime_GetClass)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *arg0 = params.args[0];

    if (!arg0) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *object_ptr;

    if (arg0->GetPointer<vm::VMObject>(&object_ptr)) {
        vm::Value result_value(vm::Value::HEAP_POINTER, { .ptr = object_ptr->GetClassPointer() });
        
        HYP_SCRIPT_RETURN(result_value);
    } else {
        HYP_SCRIPT_THROW(vm::Exception("Not an object"));
    }
}

static HYP_SCRIPT_FUNCTION(Runtime_OpenFilePointer)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::VMString *path_str = GetArgument<0, vm::VMString *>(params);
    vm::VMString *args_str = GetArgument<1, vm::VMString *>(params);

    if (!path_str || !args_str) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid arguments to fopen"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    FILE *fptr = nullptr;

    if (Memory::StrCmp(path_str->GetData(), "stdout") == 0) {
        fptr = stdout;
    } else if (Memory::StrCmp(path_str->GetData(), "stderr") == 0) {
        fptr = stderr;
    } else {
        fptr = fopen(path_str->GetData(), args_str->GetData());
    }

    if (!fptr) {
        HYP_SCRIPT_RETURN_UINT32(~0u);
    }

    const UInt32 id = ++file_pointer_map.counter;
    file_pointer_map.data[id] = fptr;

    HYP_SCRIPT_RETURN_UINT32(id);
}

static HYP_SCRIPT_FUNCTION(Runtime_CloseFilePointer)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    const UInt32 file_id = GetArgument<0, UInt32>(params);

    if (file_id == ~0u) {
        HYP_SCRIPT_RETURN_BOOLEAN(false);
    }

    const auto it = file_pointer_map.data.Find(file_id);

    if (it == file_pointer_map.data.End()) {
        HYP_SCRIPT_RETURN_BOOLEAN(false);
    }

    Int close_result = -1;
    
    if (it->second != stdout && it->second != stderr) {
        close_result = fclose(it->second);
    }

    file_pointer_map.data.Erase(it);

    HYP_SCRIPT_RETURN_BOOLEAN(close_result == 0);
}

static HYP_SCRIPT_FUNCTION(Runtime_WriteFileData)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    const UInt32 file_id = GetArgument<0, UInt32>(params);

    if (file_id == ~0u) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    const auto it = file_pointer_map.data.Find(file_id);

    if (it == file_pointer_map.data.End()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    FILE *fptr = it->second;

    if (!fptr) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    Value *target_ptr = params.args[1];

    const int buffer_size = 256;
    char buffer[buffer_size];
    std::snprintf(
        buffer,
        buffer_size,
        "Invalid argument type to write to file, %s. Argument must be one of: (String, MemoryBuffer, Struct)",
        target_ptr ? target_ptr->GetTypeString() : "NULL"
    );
    vm::Exception e(buffer);
    
    if (!target_ptr) {
        params.handler->state->ThrowException(params.handler->thread, e);

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union
        {
            VMString        *str_ptr;
            VMArray         *array_ptr;
            VMMemoryBuffer  *memory_buffer_ptr;
            VMObject        *obj_ptr;
            VMStruct        *struct_ptr;
        } data;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.str_ptr = target_ptr->GetValue().ptr->GetPointer<VMString>()) != nullptr) {
            fwrite(data.str_ptr->GetData(), 1, data.str_ptr->GetLength(), fptr);
        } else if ((data.memory_buffer_ptr = target_ptr->GetValue().ptr->GetPointer<VMMemoryBuffer>()) != nullptr) {
            fwrite(data.memory_buffer_ptr->GetBuffer(), 1, data.memory_buffer_ptr->GetSize(), fptr);
        } else if ((data.struct_ptr = target_ptr->GetValue().ptr->GetPointer<VMStruct>()) != nullptr) {
            fwrite(data.struct_ptr->GetMemory().Data(), 1, data.struct_ptr->GetMemory().Size(), fptr);
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    HYP_SCRIPT_RETURN_VOID(nullptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_FlushFileStream)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    const UInt32 file_id = GetArgument<0, UInt32>(params);

    if (file_id == ~0u) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    const auto it = file_pointer_map.data.Find(file_id);

    if (it == file_pointer_map.data.End()) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    FILE *fptr = it->second;

    if (!fptr) {
        params.handler->state->ThrowException(params.handler->thread, vm::Exception("Invalid file handle"));

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    fflush(fptr);

    HYP_SCRIPT_RETURN_VOID(nullptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_ToMemoryBuffer)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    MemoryByteWriter byte_writer;

    vm::Value *target_ptr = params.args[0];

    constexpr int buffer_size = 256;
    char buffer[buffer_size];
    std::snprintf(
        buffer,
        buffer_size,
        "Invalid argument type to convert to MemoryBuffer, %s.",
        target_ptr ? target_ptr->GetTypeString() : "NULL"
    );
    vm::Exception e(buffer);
    
    if (!target_ptr) {
        params.handler->state->ThrowException(params.handler->thread, e);

        HYP_SCRIPT_RETURN_VOID(nullptr);
    }

    union
    {
        VMString        *str_ptr;
        VMArray         *array_ptr;
        VMMemoryBuffer  *memory_buffer_ptr;
        VMObject        *obj_ptr;
        VMStruct        *struct_ptr;
    } data;

    switch (target_ptr->GetType()) {
    case Value::ValueType::HEAP_POINTER:
        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.str_ptr = target_ptr->GetValue().ptr->GetPointer<VMString>()) != nullptr) {
            byte_writer.Write(data.str_ptr->GetData(), data.str_ptr->GetLength());
        } else if ((data.memory_buffer_ptr = target_ptr->GetValue().ptr->GetPointer<VMMemoryBuffer>()) != nullptr) {
            byte_writer.Write(data.memory_buffer_ptr->GetBuffer(), data.memory_buffer_ptr->GetSize());
        } else if ((data.struct_ptr = target_ptr->GetValue().ptr->GetPointer<VMStruct>()) != nullptr) {
            byte_writer.Write(data.struct_ptr->GetMemory().Data(), data.struct_ptr->GetMemory().Size());
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }

        break;
    case Value::ValueType::I8:
        byte_writer.Write(target_ptr->m_value.i8);
        break;
    case Value::ValueType::I16:
        byte_writer.Write(target_ptr->m_value.i16);
        break;
    case Value::ValueType::I32:
        byte_writer.Write(target_ptr->m_value.i32);
        break;
    case Value::ValueType::I64:
        byte_writer.Write(target_ptr->m_value.i64);
        break;
    case Value::ValueType::U8:
        byte_writer.Write(target_ptr->m_value.u8);
        break;
    case Value::ValueType::U16:
        byte_writer.Write(target_ptr->m_value.u16);
        break;
    case Value::ValueType::U32:
        byte_writer.Write(target_ptr->m_value.u32);
        break;
    case Value::ValueType::U64:
        byte_writer.Write(target_ptr->m_value.u64);
        break;
    case Value::ValueType::BOOLEAN:
        byte_writer.Write(UInt8(target_ptr->m_value.b));
        break;
    case Value::ValueType::F32:
        byte_writer.Write(target_ptr->m_value.f);
        break;
    case Value::ValueType::F64:
        byte_writer.Write(target_ptr->m_value.d);
        break;
    default:
        params.handler->state->ThrowException(params.handler->thread, e);

        break;
    }

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(vm::VMMemoryBuffer(ByteBuffer(SizeType(byte_writer.Position()), byte_writer.GetData().Data())));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_GetMemoryAddress)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *arg0 = params.args[0];

    if (!arg0) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }

    void *vp = nullptr;

    if (arg0) {
        switch (arg0->m_type) {
        case vm::Value::HEAP_POINTER:
            if (arg0->m_value.ptr != nullptr) {
                vp = arg0->m_value.ptr->GetRawPointer();
            }

            break;
        case vm::Value::USER_DATA:
            vp = arg0->m_value.user_data;

            break;
        default:
            vp = arg0;

            break;
        }
    }

    char buffer[256] = { '\0' };
    std::snprintf(buffer, sizeof(buffer), "%p", vp);

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(VMString(buffer));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_IsInstance)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    vm::Value *arg0 = params.args[0];
    vm::Value *arg1 = params.args[1];

    if (!arg0) {
        HYP_SCRIPT_RETURN_BOOLEAN(false);
    }

    if (!arg1) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }
    
    vm::VMObject *class_object_ptr = nullptr;

    if (!arg1->GetPointer<vm::VMObject>(&class_object_ptr)) {
        HYP_SCRIPT_THROW(vm::Exception("Parameter 'cls' is not an object"));
    }

    vm::VMObject *target_ptr = nullptr;
    bool is_instance = false;

    if (arg0->GetPointer<vm::VMObject>(&target_ptr)) {
        // target is an object, we compare the base class of target
        // to the value we have in class_object_ptr.

        if (HeapValue *target_class = target_ptr->GetClassPointer()) {
            constexpr UInt max_depth = 1024;
            UInt depth = 0;

            vm::VMObject *target_class_object = target_class->GetPointer<vm::VMObject>();

            while (target_class_object != nullptr && depth < max_depth) {
                is_instance = (*target_class_object == *class_object_ptr);

                if (is_instance) {
                    break;
                }

                vm::Value base = vm::Value(vm::Value::NONE, { .ptr = nullptr });

                if (!(target_class_object->LookupBasePointer(base) && base.GetPointer<vm::VMObject>(&target_class_object))) {
                    break;
                }

                depth++;
            }

            if (depth == max_depth) {
                HYP_SCRIPT_THROW(vm::Exception("Maximum recursion depth for IsInstance() exceeded"));
            }
        }
    } else {
        Member *proto_mem = class_object_ptr->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH, false);

        if (proto_mem != nullptr) {
            // check target's type is equal to the type of $proto value,
            // since it is not an object pointer
            is_instance = (arg0->m_type == proto_mem->value.m_type);
        }
    }

    HYP_SCRIPT_RETURN_BOOLEAN(is_instance);
}

static HYP_SCRIPT_FUNCTION(EntityGetName)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    auto &&entity_handle = GetArgument<0, Handle<Entity>>(params);

    if (!entity_handle) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(VMString(entity_handle->GetName().LookupString()));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

static HYP_SCRIPT_FUNCTION(EngineCreateEntity)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    Engine *engine_ptr = (Engine *)GetArgument<0, void *>(params);

    if (!engine_ptr) {
        HYP_SCRIPT_THROW(vm::Exception::NullReferenceException());
    }

    auto entity_handle = engine_ptr->CreateObject<Entity>();

    const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Handle<Entity>>();
    AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

    const auto prototype_it = params.api_instance.class_bindings.class_prototypes.Find(class_name_it->second);
    AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.End(), "Class not registered!");

    HYP_SCRIPT_CREATE_PTR(entity_handle, result);
    vm::VMObject result_value(prototype_it->second); // construct from prototype
    HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);
    HYP_SCRIPT_CREATE_PTR(result_value, ptr);
    HYP_SCRIPT_RETURN(ptr);
}

static HYP_SCRIPT_FUNCTION(Runtime_GetFunctionBytecode)
{
    static const UInt32 invoke_hash = hash_fnv_1("$invoke");

    HYP_SCRIPT_CHECK_ARGS(==, 1);

    // get value
    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    String bytecode_str;

    vm::Value value = *target_ptr;

    { // If the value holds an object, look for $invoke
        VMObject *object_ptr = nullptr;
        Member *member_ptr = nullptr;

        if (value.GetPointer<VMObject>(&object_ptr) && (member_ptr = object_ptr->LookupMemberFromHash(invoke_hash))) {
            value = member_ptr->value;
        }
    }

    if (value.m_type != vm::Value::FUNCTION) {
        if (value.m_type == vm::Value::NATIVE_FUNCTION) {
            bytecode_str = "<Native Code>";
        } {
            char buffer[256];

            std::sprintf(
                buffer,
                "cannot convert type '%s' to bytecode",
                value.GetTypeString()
            );

            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception(buffer)
            );
        }
    } else {
        AssertThrow(params.handler->bs != nullptr);

        const SizeType pos = value.m_value.func.m_addr;
        AssertThrow(pos < params.handler->bs->Size());
        
        SourceFile source_file(
            "",
            params.handler->bs->Size()
        );
        // read into it starting at pos
        source_file.ReadIntoBuffer(
            &params.handler->bs->GetBuffer()[pos],
            params.handler->bs->Size() - pos
        );
        
        vm::BytecodeStream byte_stream = vm::BytecodeStream::FromSourceFile(source_file);

        // create DecompilationUnit
        DecompilationUnit dec;
        InstructionStream is;
        utf::utf8_stringstream ss;

        UInt8 code;
        UInt depth = 1;

        do {
            ss << std::hex << (pos + byte_stream.Position()) << std::dec << '\t';

            byte_stream.Read(&code);

            // decompile the instruction
            dec.DecodeNext(code, byte_stream, is, &ss);

            if (code == RET) {
                if (!(--depth)) {
                    break;
                }
            }
        } while (!byte_stream.Eof());
        
        bytecode_str += HYP_UTF8_TOMULTIBYTE(ss.str().data());
    }

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(vm::VMString(bytecode_str));

    vm::Value res;
    // assign register value to the allocated object
    res.m_type = vm::Value::HEAP_POINTER;
    res.m_value.ptr = ptr;

    HYP_SCRIPT_RETURN(res);
}

static HYP_SCRIPT_FUNCTION(LoadModule)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    VMString *str = GetArgument<1, VMString *>(params);

    if (str == nullptr) {
        HYP_SCRIPT_THROW(vm::Exception("Module name must be a string"));
    }

    FilePath current_file_path(params.api_instance.GetSourceFile().GetFilePath());

    if (!current_file_path.Length()) {
        current_file_path = FilePath::Current();
    }
    
    FilePath path = current_file_path.BasePath() / str->GetData();

    if (!path.Exists()) {
        path += ".hypscript";
    }

    if (!path.Exists()) {
        DebugLog(
            LogType::Error,
            "Failed to load module %s: File not found\n",
            path.Data()
        );

        HYP_SCRIPT_THROW(vm::Exception("Module not found"));
    }
    
    const UInt32 hash_code = UInt32(path.GetHashCode().Value());

    RC<DynModule> dyn_module;

    {
        const auto it = params.handler->state->m_dyn_modules.Find(hash_code);

        if (it != params.handler->state->m_dyn_modules.End()) {
            dyn_module = it->second.Lock();
        }
    }

    if (dyn_module == nullptr) {
        Reader reader;

        if (!path.Open(reader)) {
            DebugLog(
                LogType::Error,
                "Failed to load module %s: Failed to open path\n",
                path.Data()
            );

            HYP_SCRIPT_RETURN_NULL();
        } else {
            const ByteBuffer byte_buffer = reader.ReadBytes();

            SourceFile source_file(str->GetData(), reader.Max());
            source_file.ReadIntoBuffer(byte_buffer.Data(), byte_buffer.Size());

            dyn_module.Reset(new DynModule);
            UniquePtr<Script> script(new Script(source_file));

            if (script->Compile()) {
                script->Bake();
                script->Run();
            } else {
                DebugLog(
                    LogType::Error,
                    "Failed to load module %s: Compilation failed\n",
                    path.Data()
                );

                HYP_SCRIPT_RETURN_NULL();
            }

            dyn_module->ptr = std::move(script);
            params.handler->state->m_dyn_modules.Insert(hash_code, Weak<DynModule>(dyn_module));
        }
    } else {
        DebugLog(
            LogType::Info,
            "Reuse dyn module %s\n",
            path.Data()
        );
    }

    const auto class_name_it = params.api_instance.class_bindings.class_names.Find<RC<DynModule>>();
    AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

    const auto prototype_it = params.api_instance.class_bindings.class_prototypes.Find(class_name_it->second);
    AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.End(), "Class not registered!");

    HYP_SCRIPT_CREATE_PTR(std::move(dyn_module), result);
    vm::VMObject result_value(prototype_it->second); // construct from prototype
    HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);
    HYP_SCRIPT_CREATE_PTR(result_value, ptr);
    HYP_SCRIPT_RETURN(ptr);
}

static HYP_SCRIPT_FUNCTION(GetModuleExportedValue)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    RC<DynModule> dyn_module = GetArgument<0, RC<DynModule>>(params);
    VMString *name_ptr = GetArgument<1, VMString *>(params);

    if (dyn_module == nullptr || dyn_module->ptr == nullptr || name_ptr == nullptr) {
        HYP_SCRIPT_RETURN_NULL();
    }

    vm::Value out_value;

    if (!static_cast<Script *>(dyn_module->ptr.Get())->GetExportedValue(name_ptr->GetData(), &out_value)) {
        HYP_SCRIPT_RETURN_NULL();
    }

    HYP_SCRIPT_RETURN(out_value);
}

static HYP_SCRIPT_FUNCTION(NameToString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::VMObject *name_object_ptr = GetArgument<0, vm::VMObject *>(params);
    AssertThrow(name_object_ptr != nullptr);

    HYP_SCRIPT_GET_MEMBER_UINT(name_object_ptr, "hash_code", UInt64, hash_code_value);

    Name name = Name(NameID(hash_code_value));
    const ANSIString &string_value = name.LookupString();

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
    AssertThrow(ptr != nullptr);

    ptr->Assign(VMString(string_value.Data()));
    ptr->Mark();

    HYP_SCRIPT_RETURN_PTR(ptr);
}

static HYP_SCRIPT_FUNCTION(NameCreateFromString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 2);

    // auto &&name = GetArgument<0, Name>(params);
    VMString *str = GetArgument<1, VMString *>(params);


    AssertThrow(str != nullptr);

    Name name = CreateNameFromDynamicString(str->GetData());

    const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Name>();
    AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

    const auto prototype_it = params.api_instance.class_bindings.class_prototypes.Find(class_name_it->second);
    AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.End(), "Class not registered!");

    vm::VMObject result_value(prototype_it->second); // construct from prototype
    HYP_SCRIPT_SET_MEMBER(result_value, "hash_code", vm::Value(vm::Value::U64, { .u64 = name.hash_code }));
    HYP_SCRIPT_CREATE_PTR(result_value, ptr);
    HYP_SCRIPT_RETURN(ptr);
}

void ScriptBindings::DeclareAll(APIInstance &api_instance)
{
    using namespace hyperion::compiler;

    //api_instance.Module(Config::global_module_name)
   //     .Class<Handle<Entity>>("Entity", { API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })) }, { });

    api_instance.Module(Config::global_module_name)
        .Class<Name>(
            "Name",
            {
                API::NativeMemberDefine("hash_code", BuiltinTypes::UNSIGNED_INT, vm::Value(vm::Value::U64, { .u64 = 0 })),

                API::NativeMemberDefine(
                    "LookupString",
                    BuiltinTypes::STRING,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    NameToString
                ),

                API::NativeMemberDefine(
                    "$construct",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "str", BuiltinTypes::STRING, RC<AstString>(new AstString("", SourceLocation::eof)) }
                    },
                    NameCreateFromString
                )
            }
        );

    api_instance.Module(Config::global_module_name)
        .Class<RC<DynModule>>(
            "Module",
            {
                API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),

                API::NativeMemberDefine(
                    "Get",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "name", BuiltinTypes::STRING }
                    },
                    GetModuleExportedValue
                )
            },
            {
                API::NativeMemberDefine(
                    "Load",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::CLASS_TYPE },
                        { "name", BuiltinTypes::STRING }
                    },
                    LoadModule
                )
            }
        );

    api_instance.Module("vm")
        .Function(
            "ReadStackVar",
            BuiltinTypes::ANY,
            {
                { "index", BuiltinTypes::UNSIGNED_INT }
            },
            VM_ReadStackVar
        );

    api_instance.Module(Config::global_module_name)
        .Function(
            "MakeStruct",
            BuiltinTypes::ANY,
            {
                {
                    "members",
                    BuiltinTypes::ARRAY
                }
            },
            Runtime_MakeStruct
        )
        .Function(
            "GetStructMember",
            BuiltinTypes::ANY,
            {
                {
                    "struct",
                    BuiltinTypes::ANY
                },
                {
                    "member_name",
                    BuiltinTypes::STRING
                }
            },
            Runtime_GetStructMember
        )
        .Function(
            "SetStructMember",
            BuiltinTypes::BOOLEAN,
            {
                {
                    "struct",
                    BuiltinTypes::ANY
                },
                {
                    "member_name",
                    BuiltinTypes::STRING
                },
                {
                    "value",
                    BuiltinTypes::ANY
                }
            },
            Runtime_SetStructMember
        )
        .Function(
            "GetStructMemoryBuffer",
            BuiltinTypes::ANY,
            {
                {
                    "struct",
                    BuiltinTypes::ANY
                }
            },
            Runtime_GetStructMemoryBuffer
        )
        .Function(
            "fopen",
            BuiltinTypes::UNSIGNED_INT,
            {
                {
                    "path",
                    BuiltinTypes::STRING
                },
                {
                    "args",
                    BuiltinTypes::STRING
                }
            },
            Runtime_OpenFilePointer
        )
        .Function(
            "fclose",
            BuiltinTypes::BOOLEAN,
            {
                {
                    "file_id",
                    BuiltinTypes::UNSIGNED_INT
                }
            },
            Runtime_CloseFilePointer
        )
        .Function(
            "fwrite",
            BuiltinTypes::VOID_TYPE,
            {
                {
                    "file_id",
                    BuiltinTypes::UNSIGNED_INT
                },
                {
                    "data",
                    BuiltinTypes::ANY
                }
            },
            Runtime_WriteFileData
        )
        .Function(
            "fflush",
            BuiltinTypes::VOID_TYPE,
            {
                {
                    "file_id",
                    BuiltinTypes::UNSIGNED_INT
                }
            },
            Runtime_FlushFileStream
        )
        .Function(
            "ToMemoryBuffer",
            BuiltinTypes::ANY,
            {
                {
                    "obj",
                    BuiltinTypes::ANY
                }
            },
            Runtime_ToMemoryBuffer
        )
        .Function(
            "GetMemoryAddress",
            BuiltinTypes::STRING,
            {
                { "value", BuiltinTypes::ANY }
            },
            Runtime_GetMemoryAddress
        )
        .Function(
            "GetFunctionBytecode",
            BuiltinTypes::STRING,
            {
                { "value", BuiltinTypes::FUNCTION }
            },
            Runtime_GetFunctionBytecode
        )
        .Function(
            "IsInstance",
            BuiltinTypes::BOOLEAN,
            {
                { "target", BuiltinTypes::ANY },
                { "cls", BuiltinTypes::ANY },
            },
            Runtime_IsInstance
        )
        .Function(
            "GetClass",
            BuiltinTypes::ANY,
            {
                { "object", BuiltinTypes::ANY }
            },
            Runtime_GetClass
        )
        .Function(
            "HasMember",
            BuiltinTypes::BOOLEAN,
            {
                { "object", BuiltinTypes::ANY },
                { "member_name", BuiltinTypes::STRING }
            },
            Runtime_HasMember
        )
        .Function(
            "GetMember",
            BuiltinTypes::ANY,
            {
                { "object", BuiltinTypes::ANY },
                { "member_name", BuiltinTypes::STRING }
            },
            Runtime_GetMember
        )
        .Function(
            "SetMember",
            BuiltinTypes::VOID_TYPE,
            {
                { "object", BuiltinTypes::ANY },
                { "member_name", BuiltinTypes::STRING },
                { "value", BuiltinTypes::ANY }
            },
            Runtime_SetMember
        )
        .Function(
            "GetMembers",
            BuiltinTypes::ARRAY,
            {
                { "object", BuiltinTypes::ANY }
            },
            Runtime_GetMembers
        );

    api_instance.Module(Config::global_module_name)
        .Function(
            "Engine_CreateEntity",
            BuiltinTypes::ANY,
            {
                { "engine", BuiltinTypes::ANY },
            },
            EngineCreateEntity
        );

    api_instance.Module(Config::global_module_name)
        .Class<Vector2>(
            "Vector2",
            {
                API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                API::NativeMemberDefine(
                    "$construct",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "x", BuiltinTypes::FLOAT, RC<AstFloat>(new AstFloat(0.0f, SourceLocation::eof)) },
                        { "y", BuiltinTypes::FLOAT, RC<AstFloat>(new AstFloat(0.0f, SourceLocation::eof)) }
                    },
                    CxxCtor< Vector2, Float, Float > 
                ),
                API::NativeMemberDefine(
                    "operator+",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2, Vector2, const Vector2 &, &Vector2::operator+ >
                ),
                API::NativeMemberDefine(
                    "operator+=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2 &, Vector2, const Vector2 &, &Vector2::operator+= >
                ),
                API::NativeMemberDefine(
                    "operator-",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2, Vector2, const Vector2 &, &Vector2::operator- >
                ),
                API::NativeMemberDefine(
                    "operator-=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2 &, Vector2, const Vector2 &, &Vector2::operator-= >
                ),
                API::NativeMemberDefine(
                    "operator*",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2, Vector2, const Vector2 &, &Vector2::operator* >
                ),
                API::NativeMemberDefine(
                    "operator*=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2 &, Vector2, const Vector2 &, &Vector2::operator*= >
                ),
                API::NativeMemberDefine(
                    "operator/",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2, Vector2, const Vector2 &, &Vector2::operator/ >
                ),
                API::NativeMemberDefine(
                    "operator/=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2 &, Vector2, const Vector2 &, &Vector2::operator/= >
                ),
                API::NativeMemberDefine(
                    "operator==",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< bool, Vector2, const Vector2 &, &Vector2::operator== >
                ),
                API::NativeMemberDefine(
                    "operator!=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< bool, Vector2, const Vector2 &, &Vector2::operator!= >
                ),
                API::NativeMemberDefine(
                    "Length",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector2, &Vector2::Length >
                ),
                API::NativeMemberDefine(
                    "Distance",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector2, const Vector2 &, &Vector2::Distance >
                ),
                API::NativeMemberDefine(
                    "Normalize",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector2 &, Vector2, &Vector2::Normalize >
                ),
                API::NativeMemberDefine(
                    "GetX",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector2, &Vector2::GetX >
                ),
                API::NativeMemberDefine(
                    "GetY",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector2, &Vector2::GetY >
                )
            }
        );

    api_instance.Module(Config::global_module_name)
        .Class<Vector3>(
            "Vector3",
            {
                API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                API::NativeMemberDefine(
                    "$construct",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "x", BuiltinTypes::FLOAT, RC<AstFloat>(new AstFloat(0.0f, SourceLocation::eof)) },
                        { "y", BuiltinTypes::FLOAT, RC<AstFloat>(new AstFloat(0.0f, SourceLocation::eof)) },
                        { "z", BuiltinTypes::FLOAT, RC<AstFloat>(new AstFloat(0.0f, SourceLocation::eof)) }
                    },
                    CxxCtor< Vector3, Float, Float, Float > 
                ),
                API::NativeMemberDefine(
                    "operator+",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator+ >
                ),
                API::NativeMemberDefine(
                    "operator+=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3 &, Vector3, const Vector3 &, &Vector3::operator+= >
                ),
                API::NativeMemberDefine(
                    "operator-",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator- >
                ),
                API::NativeMemberDefine(
                    "operator-=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3 &, Vector3, const Vector3 &, &Vector3::operator-= >
                ),
                API::NativeMemberDefine(
                    "operator*",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator* >
                ),
                API::NativeMemberDefine(
                    "operator*=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3 &, Vector3, const Vector3 &, &Vector3::operator*= >
                ),
                API::NativeMemberDefine(
                    "operator/",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator/ >
                ),
                API::NativeMemberDefine(
                    "operator/=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3 &, Vector3, const Vector3 &, &Vector3::operator/= >
                ),
                API::NativeMemberDefine(
                    "operator==",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< bool, Vector3, const Vector3 &, &Vector3::operator== >
                ),
                API::NativeMemberDefine(
                    "operator!=",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< bool, Vector3, const Vector3 &, &Vector3::operator!= >
                ),
                API::NativeMemberDefine(
                    "Dot",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, const Vector3 &, &Vector3::Dot >
                ),
                API::NativeMemberDefine(
                    "Cross",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::Cross >
                ),
                API::NativeMemberDefine(
                    "Length",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, &Vector3::Length >
                ),
                API::NativeMemberDefine(
                    "Distance",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "other", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, const Vector3 &, &Vector3::Distance >
                ),
                API::NativeMemberDefine(
                    "Normalized",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3, Vector3, &Vector3::Normalized >
                ),
                API::NativeMemberDefine(
                    "Normalize",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Vector3 &, Vector3, &Vector3::Normalize >
                ),
                API::NativeMemberDefine(
                    "ToString",
                    BuiltinTypes::STRING,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    Vector3ToString
                ),
                API::NativeMemberDefine(
                    "GetX",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, &Vector3::GetX >
                ),
                API::NativeMemberDefine(
                    "GetY",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, &Vector3::GetY >
                ),
                API::NativeMemberDefine(
                    "GetZ",
                    BuiltinTypes::FLOAT,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< Float, Vector3, &Vector3::GetZ >
                )
                // {
                //     "SetX",
                //     BuiltinTypes::ANY,
                //     {
                //         { "self", BuiltinTypes::ANY },
                //         { "value", BuiltinTypes::FLOAT }
                //     },
                //     CxxMemberFn< Vector3 &, Vector3, Float, &Vector3::SetX >
                // },
                // {
                //     "SetY",
                //     BuiltinTypes::ANY,
                //     {
                //         { "self", BuiltinTypes::ANY },
                //         { "value", BuiltinTypes::FLOAT }
                //     },
                //     CxxMemberFn< Vector3 &, Vector3, Float, &Vector3::SetY >
                // },
                // {
                //     "SetZ",
                //     BuiltinTypes::ANY,
                //     {
                //         { "self", BuiltinTypes::ANY },
                //         { "value", BuiltinTypes::FLOAT }
                //     },
                //     CxxMemberFn< Vector3 &, Vector3, Float, &Vector3::SetZ >
                // },
                // {
                //     "operator+",
                //     BuiltinTypes::ANY,
                //     {
                //         { "self", BuiltinTypes::ANY },
                //         { "other", BuiltinTypes::ANY }
                //     },
                //     CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator+ >
                // },
                // {
                //     "operator-",
                //     BuiltinTypes::ANY,
                //     {
                //         { "self", BuiltinTypes::ANY },
                //         { "other", BuiltinTypes::ANY }
                //     },
                //     CxxMemberFn< Vector3, Vector3, const Vector3 &, &Vector3::operator- >
                // }
            }
        );


    api_instance.Module(Config::global_module_name)
        .Class<BoundingBox>(
            "BoundingBox",
            {
                API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                API::NativeMemberDefine(
                    "$construct",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY },
                        { "min", BuiltinTypes::ANY },
                        { "max", BuiltinTypes::ANY }
                    },
                    CxxCtor< BoundingBox, Vector3, Vector3 > 
                ),
                API::NativeMemberDefine(
                    "GetMin",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< const Vector3 &, BoundingBox, &BoundingBox::GetMin >
                ),
                API::NativeMemberDefine(
                    "GetMax",
                    BuiltinTypes::ANY,
                    {
                        { "self", BuiltinTypes::ANY }
                    },
                    CxxMemberFn< const Vector3 &, BoundingBox, &BoundingBox::GetMax >
                ),
            }
        );

    api_instance.Module(Config::global_module_name)
        .Variable("SCRIPT_VERSION", 200)
        .Variable("ENGINE_VERSION", 200)
#ifdef HYP_DEBUG_MODE
        .Variable("DEBUG_MODE", true)
#else
        .Variable("DEBUG_MODE", false)
#endif
        .Variable("NaN", MathUtil::NaN<Float>())
        .Function(
            "ArraySize",
            BuiltinTypes::INT,
            {
                { "self", BuiltinTypes::ANY } // one of: VMArray, VMString, VMObject
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
            BuiltinTypes::VOID_TYPE,
            {
                { "ptr", BuiltinTypes::ANY } // TODO: should be unsigned, but having conversion issues
            },
            Free
        );
}

} // namespace hyperion
