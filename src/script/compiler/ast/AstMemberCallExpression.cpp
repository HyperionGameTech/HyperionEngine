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
    const String &field_name,
    const RC<AstExpression> &target,
    const RC<AstArgumentList> &arguments,
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

    RC<AstExpression> self_target = CloneAstNode(m_target);

    RC<AstArgument> self_arg(new AstArgument(
        self_target,
        false,
        false,
        false,
        false,
        "self",
        self_target->GetLocation()
    ));

    const SizeType num_arguments = m_arguments != nullptr
        ? m_arguments->GetArguments().Size() + 1
        : 1;

    Array<RC<AstArgument>> args_with_self;
    args_with_self.Reserve(num_arguments);
    args_with_self.PushBack(self_arg);

    if (m_arguments != nullptr) {
        for (auto &arg : m_arguments->GetArguments()) {
            args_with_self.PushBack(arg);
        }
    }

    // visit each argument
    for (const RC<AstArgument> &arg : args_with_self) {
        AssertThrow(arg != nullptr);

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    if (m_symbol_type->IsAnyType()) {
        m_return_type = BuiltinTypes::ANY;
        m_substituted_args = args_with_self; // NOTE: do not clone because we don't need to visit again.
    } else {
        Optional<SymbolTypeFunctionSignature> substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
            visitor,
            mod,
            m_symbol_type,
            args_with_self,
            m_location
        );

        if (!substituted.HasValue()) {
            m_return_type = BuiltinTypes::UNDEFINED;

            // not a function type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_function,
                m_location,
                m_symbol_type->ToString(true)
            ));

            return;
        }

        AssertThrow(substituted->return_type != nullptr);
    
        m_return_type = substituted->return_type;

        // change args to be newly ordered array
        m_substituted_args = CloneAllAstNodes(substituted->params);

        // visit each argument (again, substituted)
        for (const RC<AstArgument> &arg : m_substituted_args) {
            AssertThrow(arg != nullptr);
            
            arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }

        SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
            visitor,
            mod,
            m_symbol_type,
            m_substituted_args,
            m_location
        );
    }

    // should never be empty; self is needed
    if (m_substituted_args.Empty()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location
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
    m_substituted_args.PopFront();

    // location of 'self' var
    const auto target_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // use above as self arg so PUSH
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_push));
    }

    // increment stack size for 'self' var
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    if (m_substituted_args.Any()) {
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
    }

    // now we load the member into the register, we call that
    if (m_found_index != -1) {
        chunk->Append(Compiler::LoadMemberAtIndex(
            visitor,
            mod,
            m_found_index
        ));
    } else {
        const UInt32 hash = hash_fnv_1(m_field_name.Data());

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
    }

    chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_field_name));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        nullptr, // no target -- handled above
        UInt8(m_substituted_args.Size() + 1) // call w/ self as first arg
    ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_substituted_args.Size() + 1 // pops self off stack as well
    ));

    return chunk;
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

RC<AstStatement> AstMemberCallExpression::Clone() const
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

        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
