#include <script/ScriptApi.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/vm/VM.hpp>
#include <script/Hasher.hpp>

#include <system/Debug.hpp>

namespace hyperion {

using namespace vm;
using namespace compiler;

static ScriptBindingsHolder bindings_holder { };

static SymbolTypePtr_t MakeGenericInstanceType(const API::NativeFunctionDefine &fn)
{
    const bool is_generic = fn.generic_instance_info.HasValue();

    if (!is_generic) {
        return nullptr;
    }

    return SymbolType::GenericInstance(
        BuiltinTypes::GENERIC_VARIABLE_TYPE,
        fn.generic_instance_info.Get()
    );
}

static Pair<API::NativeMemberDefine, SymbolMember_t> InitNativeMemberDefine(API::NativeMemberDefine member)
{
    if (member.member_type == API::NativeMemberDefine::MEMBER_TYPE_FUNCTION) {
        auto function_type = SymbolType::Function(
            member.fn.return_type,
            member.fn.param_types
        );

        // @TODO For generic functions we will basically have to define this
        // ```
        // func DoThing<T>(target, arg0, arg1)
        // {
        //     return target.DoThing(T.native_type_id, arg0, arg1); // <--- native function call, passing in the type id stored on the class
        // }
        // ```
        // We should have a helper function that does this for us.

        const bool is_generic = member.fn.generic_instance_info.HasValue();

        if (is_generic) {
            AssertThrowMsg(false, "Not implemented");

            // auto generic_type = MakeGenericInstanceType(member.fn);
            // AssertThrow(generic_type != nullptr);

            // Array<RC<AstParameter>> generic_params;
            // generic_params.Reserve(member.fn.generic_instance_info.Get().m_generic_args.Size());

            // for (const auto &arg : member.fn.generic_instance_info.Get().m_generic_args) {
            //     generic_params.PushBack(RC<AstParameter>(new AstParameter(
            //         arg.m_name,
            //         nullptr,
            //         CloneAstNode(arg.m_default_value),
            //         false,
            //         arg.m_is_const,
            //         arg.m_is_ref,
            //         SourceLocation::eof
            //     )));
            // }

            // generic_type->SetDefaultValue(RC<AstTemplateExpression>(new AstTemplateExpression(
            //     function_expr,
            //     generic_params,
            //     RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
            //         RC<AstVariable>(new AstVariable(
            //             member.fn.return_type->GetName(),
            //             SourceLocation::eof
            //         )),
            //         SourceLocation::eof
            //     )),
            //     SourceLocation::eof
            // )));

            // member.value_type = std::move(generic_type);
        } else {
            member.value_type = std::move(function_type);
        }

        member.value.m_type = vm::Value::NATIVE_FUNCTION;
        member.value.m_value.native_func = member.fn.ptr;
    }

    AssertThrow(member.value_type != nullptr);

    SymbolMember_t symbol_member {
        member.name,
        member.value_type,
        CloneAstNode(member.value_type->GetDefaultValue())
    };

    return {
        std::move(member),
        std::move(symbol_member)
    };
}

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


static Module *GetModule(CompilationUnit *compilation_unit, const String &module_name)
{
    if (Module *mod = compilation_unit->LookupModule(module_name)) {
        return mod;
    }

    return nullptr;
}

static RC<Identifier> CreateIdentifier(
    CompilationUnit *compilation_unit,
    Module *mod,
    const String &name
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
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
    const String &variable_name,
    const SymbolTypePtr_t &variable_type,
    UserData_t user_data
)
{
    vm::Value value;
    value.m_type = Value::USER_DATA;
    value.m_value.user_data = user_data;

    return Variable(
        variable_name,
        variable_type,
        value
    );
}

API::ModuleDefine &API::ModuleDefine::Function(
    const String &function_name,
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

API::ModuleDefine &API::ModuleDefine::Function(
    const String &function_name,
    const SymbolTypePtr_t &return_type,
    const GenericInstanceTypeInfo &generic_instance_info,
    const Array<GenericInstanceTypeInfo::Arg> &param_types,
    NativeFunctionPtr_t ptr
)
{
    m_function_defs.PushBack(NativeFunctionDefine {
        function_name,
        return_type,
        generic_instance_info,
        param_types,
        ptr
    });

    return *this;
}

void API::ModuleDefine::BindAll(
    APIInstance &api_instance,
    VM *vm,
    AstVisitor *visitor,
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
            visitor,
            compilation_unit
        );
    }

    for (auto &def : m_variable_defs) {
        BindNativeVariable(
            api_instance,
            def,
            mod,
            vm,
            visitor,
            compilation_unit
        );
    }

    for (auto &def : m_function_defs) {
        BindNativeFunction(
            api_instance,
            def,
            mod,
            vm,
            visitor,
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
    AstVisitor *visitor,
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
    AstVisitor *visitor,
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

    SymbolTypePtr_t function_type = SymbolType::Function(
        def.return_type,
        def.param_types
    );

    AssertThrow(function_type->GetDefaultValue() != nullptr);

    RC<AstExpression> current_value = CloneAstNode(function_type->GetDefaultValue());

    // If it is a generic function, create a generic instance type
    if (def.generic_instance_info.HasValue()) {
        AssertThrowMsg(false, "Not implemented");

        SymbolTypePtr_t generic_type = MakeGenericInstanceType(def);
        AssertThrow(generic_type != nullptr);

        function_type = std::move(generic_type);

        Array<RC<AstParameter>> generic_params;
        generic_params.Reserve(def.generic_instance_info.Get().m_generic_args.Size());

        for (auto &arg : def.generic_instance_info.Get().m_generic_args) {
            generic_params.PushBack(RC<AstParameter>(new AstParameter(
                arg.m_name,
                nullptr,
                CloneAstNode(arg.m_default_value),
                false,
                arg.m_is_const,
                arg.m_is_ref,
                SourceLocation::eof
            )));
        }

        current_value.Reset(new AstTemplateExpression(
            current_value,
            generic_params,
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstVariable>(new AstVariable(
                    def.return_type->GetName(),
                    SourceLocation::eof
                )),
                SourceLocation::eof
            )),
            SourceLocation::eof
        ));
    }

    // set identifier info
    ident->SetFlags(FLAG_CONST);
    ident->SetSymbolType(function_type);
    ident->SetCurrentValue(current_value);
    ident->SetStackLocation(compilation_unit->GetInstructionStream().GetStackSize());
    compilation_unit->GetInstructionStream().IncStackSize();

    // finally, push to VM
    vm->PushNativeFunctionPtr(def.ptr);
}

void API::ModuleDefine::BindType(
    APIInstance &api_instance,
    const TypeDefine &def,
    Module *mod,
    VM *vm,
    AstVisitor *visitor,
    CompilationUnit *compilation_unit
)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    const String type_name = def.name;

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

    Array<NativeMemberDefine> prototype_members;
    prototype_members.Reserve(1 + def.members.Size()); // +1 for __intern

    Array<SymbolMember_t> prototype_member_types;
    prototype_member_types.Reserve(1 + def.members.Size()); // +1 for __intern

    Array<SymbolMember_t> class_instance_member_types;
    class_instance_member_types.Reserve(2 + def.static_members.Size());

    Array<NativeMemberDefine> class_instance_members;
    class_instance_members.Reserve(2 + def.static_members.Size());

    // add static members
    for (const auto &member : def.static_members) {
        auto init_value = InitNativeMemberDefine(member);

        class_instance_members.PushBack(std::move(init_value.first));
        class_instance_member_types.PushBack(std::move(init_value.second));
    }

    { // Add "native_type_id" member if needed
        auto native_type_id_it = class_instance_members.FindIf([](const NativeMemberDefine &item)
        {
            return item.name == "native_type_id";
        });

        if (native_type_id_it == class_instance_members.End()) {
            // Add native_type_id member, set to null
            auto init_value = InitNativeMemberDefine(NativeMemberDefine(
                "native_type_id",
                BuiltinTypes::UNSIGNED_INT,
                vm::Value(vm::Value::U32, { .u32 = def.native_type_id.Value() })
            ));

            class_instance_members.PushBack(std::move(init_value.first));
            class_instance_member_types.PushBack(std::move(init_value.second));
        }
    }

    for (const auto &member : def.members) {
        auto init_value = InitNativeMemberDefine(member);

        prototype_members.PushBack(std::move(init_value.first));
        prototype_member_types.PushBack(std::move(init_value.second));
    }

    { // Add "__intern" member if needed
        auto intern_it = prototype_members.FindIf([](const NativeMemberDefine &item)
        {
            return item.name == "__intern";
        });

        if (intern_it == prototype_members.End()) {
            // Add __intern member, set to null
            auto init_value = InitNativeMemberDefine(NativeMemberDefine(
                "__intern",
                BuiltinTypes::ANY,
                vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })
            ));

            prototype_members.PushBack(std::move(init_value.first));
            prototype_member_types.PushBack(std::move(init_value.second));
        }
    }

    { // look for $proto, if not there, add it
        auto proto_it = prototype_members.FindIf([](const NativeMemberDefine &item)
        {
            return item.name == "$proto";
        });

        if (proto_it == prototype_members.End()) {
            SymbolTypePtr_t prototype_type = SymbolType::Object(
                def.name + "Instance",
                prototype_member_types,
                BuiltinTypes::OBJECT
            );

            {
                RC<AstTypeObject> prototype_type_object(new AstTypeObject(
                    prototype_type,
                    BuiltinTypes::CLASS_TYPE,
                    SourceLocation::eof
                ));

                // InitNativeMemberDefine() uses the default value of the member's SymbolType
                prototype_type->SetDefaultValue(RC<AstTypeRef>(new AstTypeRef(
                    prototype_type,
                    SourceLocation::eof
                )));

                // Push it to the ast visitor so that it will be visited,
                // also it will remain in memory as SymbolType holds a weak pointer

                // type will be registered when it is visited
                visitor->GetAstIterator()->Push(prototype_type_object);

                prototype_type->SetTypeObject(prototype_type_object);
            }

            auto init_value = InitNativeMemberDefine(NativeMemberDefine(
                "$proto",
                prototype_type,
                vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })
            ));
            
            class_instance_members.PushBack(std::move(init_value.first));
            class_instance_member_types.PushBack(std::move(init_value.second));
        }
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
    
    {
        RC<AstTypeObject> ast_type_object(new AstTypeObject(
            class_symbol_type,
            BuiltinTypes::CLASS_TYPE,
            SourceLocation::eof
        ));

        // Push it to the ast visitor so that it will be visited,
        // also it will remain in memory as SymbolType holds a weak pointer

        // type will be registered when it is visited
        visitor->GetAstIterator()->Push(ast_type_object);

        class_symbol_type->SetTypeObject(ast_type_object);
    }

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
        native_variable_define.current_value.Reset(new AstTypeRef(
            class_symbol_type,
            SourceLocation::eof
        ));

        BindNativeVariable(
            api_instance,
            native_variable_define,
            mod,
            vm,
            visitor,
            compilation_unit
        );
    }

    ExecutionThread *main_thread = vm->GetState().GetMainThread();

    vm::HeapValue *prototype_object_ptr = nullptr;

    vm::Value &class_value = main_thread->GetStack().Top();
    AssertThrow(class_value.m_value.ptr == nullptr && class_value.m_type == vm::Value::HEAP_POINTER);

    { // create prototype object in vm memory
        Array<vm::Member> member_data;
        member_data.Reserve(prototype_members.Size());

        for (const auto &member : prototype_members) {
            vm::Member member_data_instance;
            std::strncpy(member_data_instance.name, member.name.Data(), 255);
            member_data_instance.hash = hash_fnv_1(member.name.Data());
            member_data_instance.value = member.value;
            member_data.PushBack(member_data_instance);
        }

        vm::VMObject prototype_object(
            member_data.Data(),
            member_data.Size(),
            nullptr // @TODO
        );

        // create heap value for object
        prototype_object_ptr = vm->GetState().HeapAlloc(main_thread);
        AssertThrow(prototype_object_ptr != nullptr);

        api_instance.class_bindings.class_prototypes[def.name] = prototype_object_ptr;

        prototype_object_ptr->Assign(prototype_object);
        prototype_object_ptr->GetFlags() |= vm::GC_ALWAYS_ALIVE;

        auto it = class_instance_members.FindIf([](const NativeMemberDefine &item)
        {
            return item.name == "$proto";
        });

        AssertThrow(it != class_instance_members.End());
        AssertThrow(it->value.m_value.ptr == nullptr && it->value.m_type == vm::Value::HEAP_POINTER);

        it->value.m_value.ptr = prototype_object_ptr;
    }

    { // create class object in vm memory
        Array<vm::Member> member_data;
        member_data.Reserve(class_instance_members.Size());

        for (const auto &member : class_instance_members) {
            vm::Member member_data_instance;
            std::strncpy(member_data_instance.name, member.name.Data(), 255);
            member_data_instance.hash = hash_fnv_1(member.name.Data());
            member_data_instance.value = member.value;
            member_data.PushBack(member_data_instance);
        }


        vm::VMObject class_object(
            member_data.Data(),
            member_data.Size(),
            prototype_object_ptr
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
    : m_source_file(source_file),
      m_vm(nullptr)
{
}

API::ModuleDefine &APIInstance::Module(const String &name)
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

void APIInstance::BindAll(VM *vm, AstVisitor *visitor, CompilationUnit *compilation_unit)
{
    m_vm = vm;

    bindings_holder.GenerateAll(*this);

    for (auto &module_def : m_module_defs) {
        module_def.BindAll(
            *this,
            vm,
            visitor,
            compilation_unit
        );
    }
}

} // namespace hyperion
