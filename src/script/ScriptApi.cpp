#include <script/ScriptApi.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/vm/VM.hpp>

#include <system/debug.h>

namespace hyperion {

using namespace vm;
using namespace compiler;

static Module *GetModule(CompilationUnit *compilation_unit, const std::string &module_name)
{
    if (Module *mod = compilation_unit->LookupModule(module_name)) {
        return mod;
    }

    return nullptr;
}

static Identifier *CreateIdentifier(
    CompilationUnit *compilation_unit,
    Module *mod,
    const std::string &name)
{
    AssertThrow(compilation_unit != nullptr);
    AssertThrow(mod != nullptr);

    // get global scope
    Scope &scope = mod->m_scopes.Top();

    // look up variable to make sure it doesn't already exist
    // only this scope matters, variables with the same name outside
    // of this scope are fine
    Identifier *ident = mod->LookUpIdentifier(name, true);
    AssertThrowMsg(ident == nullptr, "Cannot create multiple objects with the same name");

    // add identifier
    ident = scope.GetIdentifierTable().AddIdentifier(name);

    return ident;
}

API::ModuleDefine &API::ModuleDefine::Type(const SymbolTypePtr_t &type)
{
    m_type_defs.push_back(TypeDefine {
        type
    });

    // return last
    return *this;
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    const SymbolTypePtr_t &variable_type,
    NativeInitializerPtr_t ptr)
{
    m_variable_defs.emplace_back(
        variable_name,
        variable_type,
        ptr
    );

    return *this;
}

API::ModuleDefine &API::ModuleDefine::Variable(
    const std::string &variable_name,
    const SymbolTypePtr_t &variable_type,
    UserData_t user_data)
{
    m_variable_defs.emplace_back(
        variable_name,
        variable_type,
        user_data
    );

    return *this;
}

API::ModuleDefine &API::ModuleDefine::Function(
    const std::string &function_name,
    const SymbolTypePtr_t &return_type,
    const std::vector<GenericInstanceTypeInfo::Arg> &param_types,
    NativeFunctionPtr_t ptr)
{
    m_function_defs.push_back(API::NativeFunctionDefine(
        function_name, return_type, param_types, ptr
    ));

    return *this;
}

void API::ModuleDefine::BindAll(VM *vm, CompilationUnit *compilation_unit)
{
    // look up if already created
    Module *mod = GetModule(compilation_unit, m_name);

    bool close_mod = false;

    // create new module
    if (mod == nullptr) {
        close_mod = true;

        m_mod.reset(new Module(m_name, SourceLocation::eof));
        mod = m_mod.get();

        // add this module to the compilation unit
        compilation_unit->m_module_tree.Open(mod);
        // set the link to the module in the tree
        mod->SetImportTreeLink(compilation_unit->m_module_tree.TopNode());
    }

    for (auto &def : m_variable_defs) {
        BindNativeVariable(def, mod, vm, compilation_unit);
    }

    for (auto &def : m_function_defs) {
        BindNativeFunction(def, mod, vm, compilation_unit);
    }

    for (auto &def : m_type_defs) {
        BindType(def, mod, vm, compilation_unit);
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
    const NativeVariableDefine &def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    Identifier *ident = CreateIdentifier(compilation_unit, mod, def.name);

    AssertThrow(ident != nullptr);

    // create the value (set it to the default value of the type)
    ident->SetSymbolType(def.type);
    ident->SetCurrentValue(def.type->GetDefaultValue());
    ident->SetStackLocation(compilation_unit->GetInstructionStream().GetStackSize());
    compilation_unit->GetInstructionStream().IncStackSize();

    AssertThrow(vm->GetState().GetNumThreads() != 0);

    ExecutionThread *main_thread = vm->GetState().GetMainThread();

    // create the object that will be stored
    Value obj;

    switch (def.value_type) {
        case NativeVariableDefine::INITIALIZER:
            obj.m_type = Value::HEAP_POINTER;
            obj.m_value.ptr = nullptr;
            // call the initializer
            def.value.initializer_ptr(&vm->GetState(), main_thread, &obj);
            break;

        case NativeVariableDefine::USER_DATA:
            obj.m_type = Value::USER_DATA;
            obj.m_value.user_data = def.value.user_data;
            break;
    }

    // push the object to the main thread's stack
    main_thread->GetStack().Push(obj);
}

void API::ModuleDefine::BindNativeFunction(
    const NativeFunctionDefine &def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);

    Identifier *ident = CreateIdentifier(compilation_unit, mod, def.function_name);
    AssertThrow(ident != nullptr);

    // create value
    std::vector<std::shared_ptr<AstParameter>> parameters; // TODO
    std::shared_ptr<AstBlock> block(new AstBlock(SourceLocation::eof));
    std::shared_ptr<AstFunctionExpression> value(new AstFunctionExpression(
        parameters, nullptr, block, false, false, false, SourceLocation::eof
    ));

    value->SetReturnType(def.return_type);

    std::vector<GenericInstanceTypeInfo::Arg> generic_param_types;

    generic_param_types.push_back({
        "@return", def.return_type
    });

    for (auto &it : def.param_types) {
        generic_param_types.push_back(it);
    }

    // set identifier info
    ident->SetFlags(FLAG_CONST);
    ident->SetSymbolType(SymbolType::GenericInstance(
        BuiltinTypes::FUNCTION, 
        GenericInstanceTypeInfo { generic_param_types }
    ));
    ident->SetCurrentValue(value);
    ident->SetStackLocation(compilation_unit->GetInstructionStream().GetStackSize());
    compilation_unit->GetInstructionStream().IncStackSize();

    // finally, push to VM
    vm->PushNativeFunctionPtr(def.ptr);
}

void API::ModuleDefine::BindType(TypeDefine def,
    Module *mod,
    VM *vm,
    CompilationUnit *compilation_unit)
{
    AssertThrow(mod != nullptr && vm != nullptr && compilation_unit != nullptr);
    AssertThrow(def.m_type != nullptr);

    const std::string type_name = def.m_type->GetName();

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
    } else {
        compilation_unit->RegisterType(def.m_type);
        // add the type to the identifier table, so it's usable
        Scope &top_scope = mod->m_scopes.Top();
        top_scope.GetIdentifierTable().AddSymbolType(def.m_type);
    }
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
    API::ModuleDefine def;
    def.m_name = name;

    // return the new instance
    m_module_defs.push_back(def);
    return m_module_defs.back();
}

void APIInstance::BindAll(VM *vm, CompilationUnit *compilation_unit)
{
    for (auto &module_def : m_module_defs) {
        module_def.BindAll(vm, compilation_unit);
    }
}

} // namespace hyperion
