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
    const RC<AstExpression> &target,
    const Array<RC<AstArgument>> &args,
    bool insert_self,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
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
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);

    m_substituted_args = m_args;

    if (m_insert_self) {
        if (const auto *left_target = m_target->GetTarget()) {
            const auto self_target = CloneAstNode(left_target);
            AssertThrow(self_target != nullptr);

            RC<AstArgument> self_arg(new AstArgument(
                self_target,
                false,
                true,
                Keyword::ToString(Keywords::Keyword_self),
                self_target->GetLocation()
            ));
            
            m_substituted_args.PushFront(std::move(self_arg));
        }
    }

    // allow unboxing
    SymbolTypePtr_t unboxed_type = target_type;

    if (target_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        if (target_type->IsBoxedType()) {
            unboxed_type = target_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
        }
    }

    AssertThrow(unboxed_type != nullptr);

    SymbolTypePtr_t call_member_type;
    std::string call_member_name;

    if ((call_member_type = unboxed_type->FindPrototypeMember("$invoke"))) {
        call_member_name = "$invoke";
    } else if ((call_member_type = unboxed_type->FindPrototypeMember("$construct"))) {
        call_member_name = "$construct";
    }

    if (call_member_type != nullptr) {
        // if (call_member_name == "$invoke") {
            // closure objects have a self parameter for the '$invoke' call.
            RC<AstArgument> self_arg(new AstArgument(
                m_target,
                false,
                false,
                "__closure_self",
                m_target->GetLocation()
            ));
            
            // insert at front
            m_substituted_args.PushFront(std::move(self_arg));
        // }

        m_target.Reset(new AstMember(
            call_member_name,
            CloneAstNode(m_target),
            m_location
        ));
        
        AssertThrow(m_target != nullptr);
        m_target->Visit(visitor, mod);

        unboxed_type = call_member_type;
        AssertThrow(unboxed_type != nullptr);
    }

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor,
        mod,
        unboxed_type,
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
        unboxed_type,
        m_substituted_args,
        m_location
    );

    if (substituted.first != nullptr) {
        m_return_type = substituted.first;
        // change args to be newly ordered vector
        m_substituted_args = substituted.second;
    } else {
        // not a function type
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_function,
            m_location,
            target_type->GetName()
        ));
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
    AssertThrow(m_target != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build arguments
    chunk->Append(Compiler::BuildArgumentsStart(
        visitor,
        mod,
        m_substituted_args
    ));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        m_target,
        UInt8(m_substituted_args.Size())
    ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_substituted_args.Size()
    ));

    return std::move(chunk);
}

void AstCallExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);

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
    // cannot deduce if return value is true
    return Tribool::Indeterminate();
}

bool AstCallExpression::MayHaveSideEffects() const
{
    // assume a function call has side effects
    // maybe we could detect this later
    return true;
}

SymbolTypePtr_t AstCallExpression::GetExprType() const
{
    AssertThrow(m_return_type != nullptr);
    return m_return_type;
}

AstExpression *AstCallExpression::GetTarget() const
{
    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
