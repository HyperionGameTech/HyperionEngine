#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstMemberCallExpression::AstMemberCallExpression(
    const std::string &field_name,
    const std::shared_ptr<AstExpression> &target,
    const std::shared_ptr<AstArgumentList> &arguments,
    const SourceLocation &location
) : AstMember(
        field_name,
        target,
        location
    ),
    m_arguments(arguments)
{
}

void AstMemberCallExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AstMember::Visit(visitor, mod);

    auto self_target = CloneAstNode(m_target);

    std::shared_ptr<AstArgument> self_arg(new AstArgument(
        self_target,
        false,
        true,
        Keyword::ToString(Keywords::Keyword_self),
        self_target->GetLocation()
    ));

    m_substituted_args.push_back(self_arg);

    if (m_arguments != nullptr) {
        for (auto &arg : m_arguments->GetArguments()) {
            m_substituted_args.push_back(arg);
        }
    }

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor,
        mod,
        m_symbol_type,
        m_substituted_args,
        m_location
    );

    // visit each argument
    for (auto &arg : m_substituted_args) {
        AssertThrow(arg != nullptr);

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }
    
    SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
        visitor,
        mod,
        m_symbol_type,
        m_substituted_args,
        m_location
    );

    if (substituted.first != nullptr) {
        m_return_type = substituted.first;
        // change args to be newly ordered vector
        m_substituted_args = substituted.second;
    } else {
        m_return_type = BuiltinTypes::UNDEFINED;

        // not a function type
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_function,
            m_location,
            m_symbol_type->GetName()
        ));
    }
}

std::unique_ptr<Buildable> AstMemberCallExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_target != nullptr);
    chunk->Append(m_target->Build(visitor, mod));

    AssertThrow(m_target_type != nullptr);

    // now we have to call the function. we pop the first arg from
    // m_substituted args because we have already pushed self to stack
    m_substituted_args.erase(m_substituted_args.begin());

    // location of 'self' var
    const auto target_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // use above as self arg so PUSH
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));
    }

    // increment stack size for 'self' var
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    if (!m_substituted_args.empty()) {
        // build arguments
        chunk->Append(Compiler::BuildArgumentsStart(
            visitor,
            mod,
            m_substituted_args
        ));

        int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // load our target from the stack into the current register (where we put it before)
        auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
        instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(stack_size - target_stack_location);
        chunk->Append(std::move(instr_load_offset));

        // HYP_BREAKPOINT;
    }

    // now we load the member into the register, we call that
    if (m_target_type == BuiltinTypes::ANY) {
        // for Any type we will have to load from hash
        const uint32_t hash = hash_fnv_1(m_field_name.c_str());

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
    } else {
        AssertThrow(m_found_index != -1);

        // just load the data member.
        chunk->Append(Compiler::LoadMemberAtIndex(
            visitor,
            mod,
            m_found_index
        ));
    }

    chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_field_name));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        nullptr, // no target -- handled above
        static_cast<uint8_t>(m_substituted_args.size() + 1) // call w/ self as first arg
    ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_substituted_args.size() + 1 // pops self off stack as well
    ));

    return std::move(chunk);
}

void AstMemberCallExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AstMember::Optimize(visitor, mod);

    for (auto &arg : m_substituted_args) {
        AssertThrow(arg != nullptr);

        arg->Optimize(visitor, mod);
    }

    // TODO: check if the member being accessed is constant and can
    // be optimized
}

Pointer<AstStatement> AstMemberCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstMemberCallExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstMemberCallExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstMemberCallExpression::GetExprType() const
{
    AssertThrow(m_return_type != nullptr);
    return m_return_type;
}

const AstExpression *AstMemberCallExpression::GetValueOf() const
{
    // if (m_symbol_type != nullptr && m_symbol_type->GetDefaultValue() != nullptr) {
    //     return m_symbol_type->GetDefaultValue()->GetValueOf();
    // }

    return AstExpression::GetValueOf();
}

const AstExpression *AstMemberCallExpression::GetDeepValueOf() const
{
    // if (m_symbol_type != nullptr && m_symbol_type->GetDefaultValue() != nullptr) {
    //     return m_symbol_type->GetDefaultValue()->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

AstExpression *AstMemberCallExpression::GetTarget() const
{
    if (m_target != nullptr) {
        // if (auto *nested_target = m_target->GetTarget()) {
        //     return nested_target;
        // }

        return m_target.get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
