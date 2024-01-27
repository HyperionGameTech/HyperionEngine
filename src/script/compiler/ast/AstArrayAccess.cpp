#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstArrayAccess::AstArrayAccess(
    const RC<AstExpression> &target,
    const RC<AstExpression> &index,
    const RC<AstExpression> &rhs,
    bool operator_overloading_enabled,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
    m_target(target),
    m_index(index),
    m_rhs(rhs),
    m_operator_overloading_enabled(operator_overloading_enabled)
{
}

void AstArrayAccess::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    m_target->Visit(visitor, mod);
    m_index->Visit(visitor, mod);

    if (m_rhs != nullptr) {
        m_rhs->Visit(visitor, mod);
    }

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);
    target_type = target_type->GetUnaliased();

    if (mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG)) {
        // TODO: implement
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location
        ));
    }

    // @TODO Right hand side of array should get passed to operator[] as an argument

    if (m_operator_overloading_enabled) {
        // Treat it the same as AstBinaryExpression does - look for operator[] or operator[]=
        const String overload_function_name = m_rhs != nullptr
            ? "operator[]="
            : "operator[]";

        RC<AstArgumentList> argument_list(new AstArgumentList(
            {
                RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_index),
                    false,
                    false,
                    false,
                    false,
                    "index",
                    m_location
                ))
            },
            m_location
        ));

        // add right hand side as argument if it exists
        if (m_rhs != nullptr) {
            argument_list->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                CloneAstNode(m_rhs),
                false,
                false,
                false,
                false,
                "value",
                m_location
            )));
        }

        RC<AstExpression> call_operator_overload_expr(new AstMemberCallExpression(
            overload_function_name,
            CloneAstNode(m_target),
            argument_list, // use right hand side as arg
            m_location
        ));

        // check if target is an array
        if (target_type->IsProxyClass() && target_type->FindMember(overload_function_name)) {
            RC<AstCallExpression> call_expr(new AstCallExpression(
                RC<AstMember>(new AstMember(
                    overload_function_name,
                    CloneAstNode(m_target),
                    m_location
                )),
                {
                    RC<AstArgument>(new AstArgument(
                        CloneAstNode(m_index),
                        false,
                        false,
                        false,
                        false,
                        "index",
                        m_location
                    ))
                },
                true,
                m_location
            ));

            // add right hand side as argument if it exists
            if (m_rhs != nullptr) {
                call_expr->GetArguments().PushBack(RC<AstArgument>(new AstArgument(
                    CloneAstNode(m_rhs),
                    false,
                    false,
                    false,
                    false,
                    "value",
                    m_location
                )));
            }

            m_override_expr = std::move(call_expr);
        } else if (target_type->IsAnyType() || target_type->IsPlaceholderType()) {
            auto sub_expr = Clone().CastUnsafe<AstArrayAccess>();
            sub_expr->SetIsOperatorOverloadingEnabled(false); // don't look for operator[] again
            
            m_override_expr.Reset(new AstTernaryExpression(
                RC<AstHasExpression>(new AstHasExpression(
                    CloneAstNode(m_target),
                    overload_function_name,
                    m_location
                )),
                call_operator_overload_expr,
                sub_expr,
                m_location
            ));
        } else if (target_type->FindPrototypeMemberDeep(overload_function_name)) {
            m_override_expr = std::move(call_operator_overload_expr);
        } else {
            // Add error
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_invalid_subscript,
                m_location,
                target_type->ToString()
            ));
        }

        if (m_override_expr != nullptr) {
            m_override_expr->SetAccessMode(GetAccessMode());
            m_override_expr->SetExpressionFlags(GetExpressionFlags());

            m_override_expr->Visit(visitor, mod);

            return;
        }
    }
}

std::unique_ptr<Buildable> AstArrayAccess::Build(AstVisitor *visitor, Module *mod)
{
    if (m_override_expr != nullptr) {
        return m_override_expr->Build(visitor, mod);
    }

    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool target_side_effects = m_target->MayHaveSideEffects();
    const bool index_side_effects = m_index->MayHaveSideEffects();

    if (m_rhs != nullptr) {
        chunk->Append(m_rhs->Build(visitor, mod));
    }
    
    UInt8 rp_before = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    UInt8 rp;
    UInt8 r0, r1;

    Compiler::ExprInfo info {
        m_target.Get(),
        m_index.Get()
    };

    if (!index_side_effects) {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    } else if (index_side_effects && !target_side_effects) {
        // load the index and store it
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp;
        r1 = rp - 1;
    } else {
        // load target, store it, then load the index
        chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        r0 = rp - 1;
        r1 = rp;
    }


    // unclaim register
    rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    // do the operation
    if (m_access_mode == ACCESS_MODE_LOAD) {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = LOAD_ARRAYIDX;
        instr->Accept<UInt8>(rp); // destination
        instr->Accept<UInt8>(r0); // source
        instr->Accept<UInt8>(r1); // index

        chunk->Append(std::move(instr));
    } else if (m_access_mode == ACCESS_MODE_STORE) {
        auto instr = BytecodeUtil::Make<RawOperation<>>();
        instr->opcode = MOV_ARRAYIDX_REG;
        instr->Accept<UInt8>(rp); // destination
        instr->Accept<UInt8>(r1); // index
        instr->Accept<UInt8>(rp_before - 1); // source

        chunk->Append(std::move(instr));
    }

    return chunk;
}

void AstArrayAccess::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_override_expr != nullptr) {
        m_override_expr->Optimize(visitor, mod);

        return;
    }

    AssertThrow(m_target != nullptr);
    AssertThrow(m_index != nullptr);

    m_target->Optimize(visitor, mod);
    m_index->Optimize(visitor, mod);
}

RC<AstStatement> AstArrayAccess::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayAccess::IsTrue() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstArrayAccess::MayHaveSideEffects() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->MayHaveSideEffects();
    }

    return m_target->MayHaveSideEffects() || m_index->MayHaveSideEffects()
        || (m_rhs != nullptr && m_rhs->MayHaveSideEffects())
        || m_access_mode == ACCESS_MODE_STORE;
}

SymbolTypePtr_t AstArrayAccess::GetExprType() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetExprType();
    }

    if (m_rhs != nullptr) {
        return m_rhs->GetExprType();
    }

    AssertThrow(m_target != nullptr);

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);
    target_type = target_type->GetUnaliased();

    if (target_type->IsAnyType()) {
        return BuiltinTypes::ANY;
    }

    if (target_type->IsPlaceholderType()) {
        return BuiltinTypes::PLACEHOLDER;
    }

    return BuiltinTypes::ANY;
}

AstExpression *AstArrayAccess::GetTarget() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetTarget();
    }

    if (m_target != nullptr) {
        if (auto *nested_target = m_target->GetTarget()) {
            return nested_target;
        }

        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstArrayAccess::IsMutable() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->IsMutable();
    }

    AssertThrow(m_target != nullptr);

    if (!m_target->IsMutable()) {
        return false;
    }

    return true;
}

const AstExpression *AstArrayAccess::GetValueOf() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetValueOf();
    }

    if (m_rhs != nullptr) {
        return m_rhs->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstArrayAccess::GetDeepValueOf() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetDeepValueOf();
    }

    if (m_rhs != nullptr) {
        return m_rhs->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
