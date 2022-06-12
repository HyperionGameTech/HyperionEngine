#include <script/compiler/ast/AstMetaBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/ScriptApi.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/aex-builder/AEXGenerator.hpp>

#include <script/vm/VM.hpp>
#include <script/vm/InstructionHandler.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/TypeInfo.hpp>

#include <script/Hasher.hpp>

#include <functional>
#include <cstdint>
#include <iostream>

namespace hyperion::compiler {

AstMetaBlock::AstMetaBlock(
    const std::vector<std::shared_ptr<AstStatement>> &children,
    const SourceLocation &location)
    : AstStatement(location),
      m_children(children)
{
}

void AstMetaBlock::Visit(AstVisitor *visitor, Module *mod)
{
    struct MetaContext {
        AstVisitor *m_visitor;
        Module *m_mod;
    } meta_context;

    meta_context.m_visitor = visitor;
    meta_context.m_mod = mod;

    AstIterator ast_iterator;

    // visit all children in the meta block
    for (auto &child : m_children) {
        AssertThrow(child != nullptr);
        ast_iterator.Push(child);
    }

    vm::VM vm;
    CompilationUnit compilation_unit;

    APIInstance meta_api;

    meta_api.Module(compiler::Config::global_module_name)
        .Variable("__meta_context", BuiltinTypes::ANY, (UserData_t)&meta_context)
        .Variable("scope", BuiltinTypes::ANY, [](vm::VMState *state, vm::ExecutionThread *thread, vm::Value *out) {
            AssertThrow(state != nullptr);
            AssertThrow(out != nullptr);

            NativeFunctionPtr_t lookup_func = [](hyperion::sdk::Params params) {
                HYP_SCRIPT_CHECK_ARGS(==, 3); // 3 args because first should be self (meta_state)

                vm::Exception e("lookup() expects arguments of type MetaContext and String");

                vm::Value *arg1 = params.args[1];
                AssertThrow(arg1 != nullptr);
                MetaContext *meta_context = (MetaContext*)arg1->m_value.user_data;

                vm::Value *arg2 = params.args[2];
                AssertThrow(arg2 != nullptr);

                if (vm::ImmutableString *str_ptr = arg2->GetValue().ptr->GetPointer<vm::ImmutableString>()) {
                    if (Identifier *ident = meta_context->m_mod->LookUpIdentifier(std::string(str_ptr->GetData()), false, true)) {
                        // @TODO return a "Variable" instance that gives info on the variable
                        std::cout << "found " << ident->GetName() << "\n";
                    } else {
                        // not found... return null
                        vm::Value res;
                        res.m_type = vm::Value::HEAP_POINTER;
                        res.m_value.ptr = nullptr;

                        HYP_SCRIPT_RETURN(res);
                    }
                } else {
                    HYP_SCRIPT_THROW(e);
                }
            };

            vm::Member members[1]; // TODO make a builder class or something to do this
            std::strcpy(members[0].name, "lookup");
            members[0].hash = hash_fnv_1("lookup");
            members[0].value.m_type = vm::Value::NATIVE_FUNCTION;
            members[0].value.m_value.native_func = lookup_func;

            // create prototype object.
            vm::HeapValue *proto = state->HeapAlloc(thread);
            AssertThrow(proto != nullptr);
            proto->Assign(vm::Object(&members[0], sizeof(members) / sizeof(members[0])));

            // create Object instance
            vm::Object ins(proto);

            vm::HeapValue *object = state->HeapAlloc(thread);
            AssertThrow(object != nullptr);
            object->Assign(ins);
            
            // assign the out value to this
            out->m_type = vm::Value::ValueType::HEAP_POINTER;
            out->m_value.ptr = object;

            object->Mark();
        });
    
    meta_api.BindAll(&vm, &compilation_unit);

    SemanticAnalyzer meta_analyzer(&ast_iterator, &compilation_unit);
    meta_analyzer.Analyze();

    if (!compilation_unit.GetErrorList().HasFatalErrors()) {
        // build in-place
        Compiler meta_compiler(&ast_iterator, &compilation_unit);
        ast_iterator.ResetPosition();

        std::unique_ptr<BytecodeChunk> result = meta_compiler.Compile();

        BuildParams build_params;
        build_params.block_offset = 0;
        build_params.local_offset = 0;

        AEXGenerator gen(build_params);
        gen.Visit(result.get());

        std::vector<std::uint8_t> bytes = gen.GetInternalByteStream().Bake();

        vm::BytecodeStream bs(reinterpret_cast<char*>(&bytes[0]), bytes.size());

        vm.Execute(&bs);
    }

    // concatenate errors to this compilation unit
    visitor->GetCompilationUnit()->GetErrorList().Concatenate(compilation_unit.GetErrorList());
}

std::unique_ptr<Buildable> AstMetaBlock::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstMetaBlock::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstMetaBlock::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
