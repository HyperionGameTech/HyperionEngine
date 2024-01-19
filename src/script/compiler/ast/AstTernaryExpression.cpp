#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <cstdio>

namespace hyperion {
namespace compiler {

AstTernaryExpression::AstTernaryExpression(
    const RC<AstExpression> &conditional,
    const RC<AstExpression> &left,
    const RC<AstExpression> &right,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_conditional(conditional),
    m_left(left),
    m_right(right)
{
}

void AstTernaryExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    m_conditional->Visit(visitor, mod);

    // TEMPORARY HACK UNTIL SOMETHING LIKE `AstCompileTimeTernary` exists.

    if (m_conditional->IsTrue() != TRI_FALSE) {
        m_left->Visit(visitor, mod);
    }

    if (m_conditional->IsTrue() != TRI_TRUE) {
        m_right->Visit(visitor, mod);
    }

    if (GetExprType() == BuiltinTypes::UNDEFINED) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_mismatched_types,
            m_location,
            m_left->GetExprType()->ToString(),
            m_right->GetExprType()->ToString()
        ));
    }
}

std::unique_ptr<Buildable> AstTernaryExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int condition_is_true = m_conditional->IsTrue();

    if (condition_is_true == -1) {
        // the condition cannot be determined at compile time
        chunk->Append(Compiler::CreateConditional(
            visitor,
            mod,
            m_conditional.Get(),
            m_left.Get(),
            m_right.Get()
        ));
    } else if (condition_is_true) {
        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // enter the block
        chunk->Append(m_left->Build(visitor, mod));
        // do not accept the else-block
    } else {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }

        chunk->Append(m_right->Build(visitor, mod));
    }

    return chunk;
}

void AstTernaryExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    m_conditional->Optimize(visitor, mod);
    m_left->Optimize(visitor, mod);
    m_right->Optimize(visitor, mod);
}

RC<AstStatement> AstTernaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTernaryExpression::IsTrue() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    switch (int(m_conditional->IsTrue())) {
    case TRI_INDETERMINATE:
        return Tribool::Indeterminate();
    case TRI_FALSE:
        return m_right->IsTrue();
    case TRI_TRUE:
        return m_left->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTernaryExpression::MayHaveSideEffects() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    if (m_conditional->MayHaveSideEffects()) {
        return true;
    }

    switch (int(m_conditional->IsTrue())) {
    case TRI_INDETERMINATE:
        return m_left->MayHaveSideEffects() || m_right->MayHaveSideEffects();
    case TRI_TRUE:
        return m_left->MayHaveSideEffects();
    case TRI_FALSE:
        return m_right->MayHaveSideEffects();
    }

    return false;
}

SymbolTypePtr_t AstTernaryExpression::GetExprType() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    switch (int(m_conditional->IsTrue())) {
    case TRI_INDETERMINATE: {
        AssertThrow(m_left != nullptr);

        SymbolTypePtr_t l_type_ptr = m_left->GetExprType();
        AssertThrow(l_type_ptr != nullptr);

        if (m_right != nullptr) {
            // the right was not optimized away,
            // return type promotion
            SymbolTypePtr_t r_type_ptr = m_right->GetExprType();
            AssertThrow(r_type_ptr != nullptr);

            return SymbolType::TypePromotion(l_type_ptr, r_type_ptr);
        } else {
            // right was optimized away, return only left type
            return l_type_ptr;
        }
    }
    case TRI_TRUE:
        return m_left->GetExprType();
    case TRI_FALSE:
        return m_right->GetExprType();
    }

    return nullptr;
}

bool AstTernaryExpression::IsLiteral() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    const auto conditional_value = m_conditional->IsTrue();

    if (conditional_value == Tribool::Indeterminate()) {
        return false;
    }

    if (conditional_value == Tribool::True()) {
        return m_left->IsLiteral();
    }

    return m_right->IsLiteral();
}

const AstExpression *AstTernaryExpression::GetValueOf() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    const auto conditional_value = m_conditional->IsTrue();

    if (conditional_value == Tribool::Indeterminate()) {
        return AstExpression::GetValueOf();
    }

    if (conditional_value == Tribool::True()) {
        return m_left->GetValueOf();
    }

    return m_right->GetValueOf();
}

const AstExpression *AstTernaryExpression::GetDeepValueOf() const
{
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_left != nullptr);
    AssertThrow(m_right != nullptr);

    const auto conditional_value = m_conditional->IsTrue();

    if (conditional_value == Tribool::Indeterminate()) {
        return AstExpression::GetDeepValueOf();
    }

    if (conditional_value == Tribool::True()) {
        return m_left->GetDeepValueOf();
    }

    return m_right->GetDeepValueOf();
}

} // namespace compiler
} // namespace hyperion
