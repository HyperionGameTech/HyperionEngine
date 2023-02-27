#include <script/ScriptApi.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/vm/VM.hpp>
#include <script/Hasher.hpp>

#include <system/Debug.hpp>

namespace hyperion {

using namespace vm;
using namespace compiler;

static ScriptBindingsHolder bindings_holder { };

ScriptBindingsBase::ScriptBindingsBase(TypeID type_id)
{
    bindings_holder.AddBinding(this);
}


void ScriptBindingsHolder::AddBinding(ScriptBindingsBase *script_bindings)
{
    const UInt index = binding_index++;

    AssertThrowMsg(index < max_bindings, "Too many script bindings attached.");

    bindings[index] = script_bindings;
}

void ScriptBindingsHolder::GenerateAll(APIInstance &api_instance)
{
    for (auto it = bindings.Begin(); it != bindings.End(); ++it) {
        if (*it == nullptr) {
            break;
        }

        (*it)->Generate(api_instance);
    }
}


static Module *GetModule(CompilationUnit *compilation_unit, const std::string &module_name)
{
    if (Module *mod = compilation_unit->LookupModule(module_name)) {
        return mod;
    }

    return nullptr;
}

static RC<Identifier> CreateIdentifier(
    CompilationUnit *compilation_unit,
    Module *mod,
    const std::string &name
)
{
    AssertThrow(compilation_unit != nullptr);
    AssertThrow(mod != nullptr);

    // get global scope
    Scope &scope = mod->m_scopes.Top();

    // look up variable to make sure it doesn't already exist
    // only this scope matters, variables with the same name outside
    // of this scope are fine
    RC<Identifier> ident = mod->LookUpIdentifier(name, true);
    AssertThrowMsg(ident == nullptr, "Cannot create multiple objects with the same name");

    // add identifier
    ident = scope.GetIdentifierTable().AddIdentifier(name);

    return ident;
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    const SymbolTypePtr_t &variable_type,
    NativeInitializerPtr_t ptr
)
{
    m_variable_defs.PushBack(NativeVariableDefine(
        variable_name,
        variable_type,
        ptr
    ));

    return *this;
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    const SymbolTypePtr_t &variable_type,
    const vm::Value &value
)
{
    m_variable_defs.PushBack(NativeVariableDefine(
        variable_name,
        variable_type,
        value
    ));

    return *this;
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    Int32 value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::INT,
        vm::Value(Value::I32, { .i32 = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    Int64 value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::INT,
        vm::Value(Value::I64, { .i64 = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    UInt32 value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::UNSIGNED_INT,
        vm::Value(Value::U32, { .u32 = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    UInt64 value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::UNSIGNED_INT,
        vm::Value(Value::U64, { .u64 = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    Float value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::FLOAT,
        vm::Value(Value::F32, { .f = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    bool value
)
{
    return Variable(
        variable_name,
        BuiltinTypes::BOOLEAN,
        vm::Value(Value::BOOLEAN, { .b = value })
    );
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    const SymbolTypePtr_t &variable_type,
    UserData_t user_data
)
{
    vm::Value value;
    value.m_type            = Value::USER_DATA;
    value.m_value.user_data = user_data;

    return Variable(
        variable_name,
        variable_type,
        value
    );
}

API::ModuleDefine &API::ModuleDefine::Function(
    const std::string &function_name,
    const SymbolTypePtr_t &return_type,
    const Array<GenericInstanceTypeInfo::Arg> &param_types,
    NativeFunctionPtr_t ptr
)
{
    m_function_defs.PushBack(NativeFunctionDefine {
        function_name,
        return_type,
        param_types,
        ptr
    });

    return *this;
}

void API::ModuleDefine::BindAll(
    APIInstance &api_instance,
    VM *vm,
    CompilationUnit *compilation_unit
)
{
    // look up if already created
    Module *mod = GetModule(compilation_unit, m_name);

    bool close_mod = false;

    // create new module
    if (mod == nullptr) {
        close_mod = true;

        m_mod.Reset(new Module(m_name, SourceLocation::eof));
        mod = m_mod.Get();

        // add this module to the compilation unit
        compilation_unit->m_module_tree.Open(mod);
        // set the link to the module in the tree
        mod->SetImportTreeLink(compilation_unit->m_module_tree.TopNode());
    }

    for (auto &def : m_type_defs) {
        BindType(
            api_instance,
            def,
            mod,
            vm,
            compilation_unit
        );
    }

    for (auto &def : m_variable_defs) {
        BindNativeVariable(
            api_instance,
            def,
            mod,
            vm,
            compilation_unit
        );
    }

    for (auto &def : m_function_defs) {
        BindNativeFunction(
            api_instance,
            def,
            mod,
            vm,
            compilation_unit
        );
    }

    if (close_mod) {
        AssertThrow(mod != nullptr);
        // close scope for module
       // mod->m_scopes.Close();
        // close this module
        compilation_unit->m_module_tree.Close();
    }
}

void API::ModuleDefine::BindNativeVariable(
    APIInstance &api_instance,
    NativeVariableDefine &def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit
)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    RC<Identifier> ident = CreateIdentifier(compilation_unit, mod, def.name);

    AssertThrow(ident != nullptr);

    // create the value (set it to the default value of the type)
    
    if (def.is_const) {
        ident->SetFlags(ident->GetFlags() | FLAG_CONST);
    }

    if (def.current_value == nullptr) {
        def.current_value = def.type->GetDefaultValue();
    }

    ident->SetSymbolType(def.type);
    ident->SetCurrentValue(def.current_value);
    ident->SetStackLocation(compilation_unit->GetInstructionStream().GetStackSize());
    compilation_unit->GetInstructionStream().IncStackSize();

    AssertThrow(vm->GetState().GetNumThreads() != 0);

    ExecutionThread *main_thread = vm->GetState().GetMainThread();

    switch (def.value_type) {
        case NativeVariableDefine::INITIALIZER: {
            def.value.m_type = Value::HEAP_POINTER;
            def.value.m_value.ptr = nullptr;
            // call the initializer
            def.initializer_ptr(&vm->GetState(), main_thread, &def.value);
            break;
        }
        case NativeVariableDefine::VALUE:
            break;
    }

    // push the object to the main thread's stack
    main_thread->GetStack().Push(def.value);
}

void API::ModuleDefine::BindNativeFunction(
    APIInstance &api_instance,
    NativeFunctionDefine &def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit
)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    RC<Identifier> ident = CreateIdentifier(compilation_unit, mod, def.function_name);
    AssertThrow(ident != nullptr);

    // create value
    // Array<RC<AstParameter>> parameters; // TODO
    // RC<AstBlock> block(new AstBlock(SourceLocation::eof));
    // RC<AstFunctionExpression> value(new AstFunctionExpression(
    //     parameters, nullptr, block, false, false, false, SourceLocation::eof
    // ));

    // value->SetReturnType(def.return_type);

    //Array<GenericInstanceTypeInfo::Arg> generic_param_types;

    // generic_param_types.PushBack({
    //     "@return", def.return_type
    // });

    // for (auto &it : def.param_types) {
    //     generic_param_types.PushBack(it);
    // }

    auto function_type = SymbolType::Function(
        def.return_type,
        def.param_types
    );

    // set identifier info
    ident->SetFlags(FLAG_CONST);
    ident->SetSymbolType(function_type);
    ident->SetCurrentValue(function_type->GetDefaultValue());
    ident->SetStackLocation(compilation_unit->GetInstructionStream().GetStackSize());
    compilation_unit->GetInstructionStream().IncStackSize();

    // finally, push to VM
    vm->PushNativeFunctionPtr(def.ptr);
}

void API::ModuleDefine::BindType(
    APIInstance &api_instance,
    TypeDefine def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit
)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    const std::string type_name = def.name;

    if (mod->LookupSymbolType(type_name)) {
        // error; redeclaration of type in module
        compilation_unit->GetErrorList().AddError(
            CompilerError(
                LEVEL_ERROR,
                Msg_redefined_type,
                SourceLocation::eof,
                type_name
            )
        );

        return;
    }

    SymbolTypePtr_t class_symbol_type;

    Array<SymbolMember_t> prototype_member_types;
    prototype_member_types.Reserve(def.members.Size());

    Array<SymbolMember_t> class_instance_member_types;
    class_instance_member_types.Reserve(2 + def.static_members.Size());

    Array<NativeMemberDefine> class_instance_members;
    class_instance_members.Reserve(2 + def.static_members.Size());

    // add static members

    for (auto &member : def.static_members) {
        if (member.member_type == NativeMemberDefine::MEMBER_TYPE_FUNCTION) {
            member.value_type = SymbolType::Function(
                member.fn.return_type,
                member.fn.param_types
            );

            member.value.m_type = vm::Value::NATIVE_FUNCTION;
            member.value.m_value.native_func = member.fn.ptr;
        }

        AssertThrow(member.value_type != nullptr);

        class_instance_member_types.PushBack(SymbolMember_t {
            member.name,
            member.value_type,
            member.value_type->GetDefaultValue()
        });

        class_instance_members.PushBack(NativeMemberDefine(
            member.name,
            member.value_type,
            member.value
        ));
    }

    for (auto &member : def.members) {
        if (member.member_type == NativeMemberDefine::MEMBER_TYPE_FUNCTION) {
            member.value_type = SymbolType::Function(
                member.fn.return_type,
                member.fn.param_types
            );

            member.value.m_type = vm::Value::NATIVE_FUNCTION;
            member.value.m_value.native_func = member.fn.ptr;
        }

        AssertThrow(member.value_type != nullptr);

        prototype_member_types.PushBack(SymbolMember_t {
            member.name,
            member.value_type,
            member.value_type->GetDefaultValue()
        });
    }

    // look for $proto, if not there, add it
    auto proto_it = def.members.FindIf([](const NativeMemberDefine &item) {
        return item.name == "$proto";
    });

    if (proto_it == def.members.End()) {
        auto proto_type = SymbolType::Object(
            def.name + "Instance",
            prototype_member_types,
            BuiltinTypes::OBJECT
        );

        class_instance_members.PushBack(NativeMemberDefine(
            "$proto",
            proto_type,
            vm::Value(vm::Value::HEAP_POINTER, {.ptr = nullptr})
        ));

        class_instance_member_types.PushBack(SymbolMember_t {
            "$proto",
            proto_type,
            RC<AstTypeObject>(new AstTypeObject(
                proto_type,
                nullptr,
                SourceLocation::eof
            ))
        });
    }

    if (def.base_class != nullptr) {
        class_symbol_type = SymbolType::Extend(
            def.name,
            def.base_class,
            class_instance_member_types
        );
    } else {
        class_symbol_type = SymbolType::Extend(
            def.name,
            BuiltinTypes::CLASS_TYPE,
            class_instance_member_types
        );
    }

    AssertThrow(class_symbol_type != nullptr);
    AssertThrow(class_symbol_type->GetBaseType() != nullptr);

    compilation_unit->RegisterType(class_symbol_type);
    // add the type to the identifier table, so it's usable
    Scope &top_scope = mod->m_scopes.Top();
    top_scope.GetIdentifierTable().AddSymbolType(class_symbol_type);

    vm::Value initial_value;
    initial_value.m_type = vm::Value::HEAP_POINTER;
    initial_value.m_value.ptr = nullptr;

    { // create + bind the native variable
        NativeVariableDefine native_variable_define(
            def.name,
            class_symbol_type->GetBaseType(),
            initial_value,
            true // is const
        );

        // set the value of the constant so that code can use it
        native_variable_define.current_value.Reset(new AstTypeObject(
            class_symbol_type,
            nullptr,
            SourceLocation::eof
        ));

        BindNativeVariable(
            api_instance,
            native_variable_define,
            mod,
            vm,
            compilation_unit
        );
    }

    ExecutionThread *main_thread = vm->GetState().GetMainThread();

    vm::HeapValue *prototype_ptr = nullptr;

    vm::Value &class_value = main_thread->GetStack().Top();
    AssertThrow(class_value.m_value.ptr == nullptr && class_value.m_type == vm::Value::HEAP_POINTER);

    { // create prototype object in vm memory
        Array<vm::Member> member_data;
        member_data.Reserve(def.members.Size());

        for (auto &member : def.members) {
            vm::Member member_data_instance;
            std::strncpy(member_data_instance.name, member.name.c_str(), 255);
            member_data_instance.hash = hash_fnv_1(member.name.c_str());
            member_data_instance.value = member.value;
            member_data.PushBack(member_data_instance);
        }

        vm::VMObject prototype_object(
            member_data.Data(),
            member_data.Size(),
            nullptr // @TODO
        );

        // create heap value for object
        prototype_ptr = vm->GetState().HeapAlloc(main_thread);

        api_instance.class_bindings.class_prototypes[def.name] = prototype_ptr;

        AssertThrow(prototype_ptr != nullptr);
        prototype_ptr->Assign(prototype_object);
        prototype_ptr->GetFlags() |= vm::GC_ALWAYS_ALIVE;
        // prototype_ptr->Mark();

        auto it = class_instance_members.FindIf([prototype_ptr](const NativeMemberDefine &item) {
            return item.name == "$proto";
        });

        AssertThrow(it != class_instance_members.End());
        AssertThrow(it->value.m_value.ptr == nullptr && it->value.m_type == vm::Value::HEAP_POINTER);

        it->value.m_value.ptr = prototype_ptr;
    }

    { // create class object in vm memory
        Array<vm::Member> member_data;
        member_data.Reserve(class_instance_members.Size());

        for (auto &member : class_instance_members) {
            vm::Member member_data_instance;
            std::strncpy(member_data_instance.name, member.name.c_str(), 255);
            member_data_instance.hash = hash_fnv_1(member.name.c_str());
            member_data_instance.value = member.value;
            member_data.PushBack(member_data_instance);
        }


        vm::VMObject class_object(
            member_data.Data(),
            member_data.Size(),
            prototype_ptr
        );

        // create heap value for object
        vm::HeapValue *ptr = vm->GetState().HeapAlloc(main_thread);

        AssertThrow(ptr != nullptr);
        ptr->Assign(class_object);

        class_value.m_type = vm::Value::HEAP_POINTER;
        class_value.m_value.ptr = ptr;

        ptr->Mark();
    }
}

APIInstance::APIInstance(const SourceFile &source_file)
    : m_source_file(source_file)
{
}

API::ModuleDefine &APIInstance::Module(const std::string &name)
{
    // check if created already first
    for (auto &def : m_module_defs) {
        if (def.m_name == name) {
            return def;
        }
    }

    // not found, add module
    m_module_defs.PushBack(API::ModuleDefine(*this, name));

    return m_module_defs.Back();
}

void APIInstance::BindAll(VM *vm, CompilationUnit *compilation_unit)
{
    bindings_holder.GenerateAll(*this);

    for (auto &module_def : m_module_defs) {
        module_def.BindAll(
            *this,
            vm,
            compilation_unit
        );
    }
}

} // namespace hyperion
