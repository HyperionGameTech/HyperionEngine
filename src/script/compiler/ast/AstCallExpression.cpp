#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <math/MathUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <limits>
#include <iostream>

namespace hyperion::compiler {

AstCallExpression::AstCallExpression(
    const RC<AstExpression> &expr,
    const Array<RC<AstArgument>> &args,
    bool insert_self,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_args(args),
    m_insert_self(insert_self),
    m_return_type(BuiltinTypes::UNDEFINED)
{
    for (auto &arg : m_args) {
        AssertThrow(arg != nullptr);
    }
}

void AstCallExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_is_visited);
    m_is_visited = true;

    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_expr->GetExprType();
    AssertThrow(target_type != nullptr);

    Array<RC<AstArgument>> args_with_self = m_args;

    if (m_insert_self) {
        auto *value_of = m_expr->GetValueOf();
        AssertThrow(value_of != nullptr);

        if (const auto *left_target = m_expr->GetTarget()) {
            const auto self_target = CloneAstNode(left_target);
            AssertThrow(self_target != nullptr);

            RC<AstArgument> self_arg(new AstArgument(
                self_target,
                false,
                false,
                false,
                false,
                "self",
                self_target->GetLocation()
            ));
            
            args_with_self.PushFront(std::move(self_arg));
        }
    }

    SymbolTypePtr_t unaliased = target_type->GetUnaliased();
    AssertThrow(unaliased != nullptr);

    SymbolTypePtr_t call_member_type;
    String call_member_name;

    // check if $invoke is found on the object or its prototype
    if ((call_member_type = unaliased->FindMember("$invoke")) || (call_member_type = unaliased->FindPrototypeMember("$invoke"))) {
        call_member_name = "$invoke";
    }

    if (call_member_type != nullptr) {
        // closure objects have a self parameter for the '$invoke' call.
        RC<AstArgument> closure_self_arg(new AstArgument(
            CloneAstNode(m_expr),
            false,
            false,
            false,
            false,
            "$functor",
            m_expr->GetLocation()
        ));
        
        // insert at front
        args_with_self.PushFront(std::move(closure_self_arg));

        m_override_expr.Reset(new AstCallExpression(
            RC<AstMember>(new AstMember(
                call_member_name,
                CloneAstNode(m_expr),
                m_location
            )),
            CloneAllAstNodes(args_with_self),
            false,
            m_location
        ));
        
        m_override_expr->Visit(visitor, mod);

        unaliased = call_member_type->GetUnaliased();
        AssertThrow(unaliased != nullptr);
    }

    if (m_override_expr != nullptr) {
        return;
    }

    // visit each argument
    for (const RC<AstArgument> &arg : args_with_self) {
        AssertThrow(arg != nullptr);

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    if (unaliased->IsAnyType()) {
        m_return_type = BuiltinTypes::ANY;
        m_substituted_args = args_with_self; // NOTE: do not clone because we don't need to visit again.
    } else {
        Optional<SymbolTypeFunctionSignature> substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
            visitor,
            mod,
            unaliased,
            args_with_self,
            m_location
        );

        if (!substituted.HasValue()) {
            // not a function type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_function,
                m_location,
                target_type->ToString(true)
            ));

            return;
        }

        AssertThrow(substituted->return_type != nullptr);

        m_return_type = substituted->return_type;

        // change args to be newly ordered array
        m_substituted_args = CloneAllAstNodes(substituted->params);

        for (const RC<AstArgument> &arg : m_substituted_args) {
            arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }

        SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
            visitor,
            mod,
            unaliased,
            m_substituted_args,
            m_location
        );
    }

    if (m_substituted_args.Size() > MathUtil::MaxSafeValue<UInt8>()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_maximum_number_of_arguments,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstCallExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    if (m_override_expr != nullptr) {
        return m_override_expr->Build(visitor, mod);
    }
    
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build arguments
    chunk->Append(Compiler::BuildArgumentsStart(
        visitor,
        mod,
        m_substituted_args
    ));

    const Int stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        m_expr,
        UInt8(m_substituted_args.Size())
    ));

    const Int stack_size_now = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    AssertThrowMsg(
        stack_size_now == stack_size_before,
        "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
        stack_size_now,
        stack_size_before
    );

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_substituted_args.Size()
    ));

    return chunk;
}

void AstCallExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_override_expr != nullptr) {
        m_override_expr->Optimize(visitor, mod);

        return;
    }

    // optimize each argument
    for (auto &arg : m_substituted_args) {
        if (arg != nullptr) {
            arg->Optimize(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }
    }
}

RC<AstStatement> AstCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstCallExpression::IsTrue() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->IsTrue();
    }

    // cannot deduce if return value is true
    return Tribool::Indeterminate();
}

bool AstCallExpression::MayHaveSideEffects() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->MayHaveSideEffects();
    }

    // assume a function call has side effects
    // maybe we could detect this later
    return true;
}

SymbolTypePtr_t AstCallExpression::GetExprType() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetExprType();
    }

    AssertThrow(m_return_type != nullptr);
    return m_return_type;
}

AstExpression *AstCallExpression::GetTarget() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetTarget();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
