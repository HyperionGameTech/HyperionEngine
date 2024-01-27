#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

AstTypeOfExpression::AstTypeOfExpression(
    const RC<AstExpression> &expr,
    const SourceLocation &location
) : AstPrototypeSpecification(expr, location)
{
}

void AstTypeOfExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_held_type = BuiltinTypes::UNDEFINED;

    auto *value_of = m_expr->GetDeepValueOf();
    AssertThrow(value_of != nullptr);

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    if (SymbolTypePtr_t expr_type = value_of->GetExprType()) {
        m_held_type = expr_type->GetUnaliased();
    }

    AssertThrow(m_held_type != nullptr);

    m_type_ref.Reset(new AstTypeRef(
        m_held_type,
        m_location
    ));

    m_type_ref->Visit(visitor, mod);
#else
    m_symbol_type = BuiltinTypes::STRING;

    SymbolTypePtr_t expr_type;
    SymbolTypePtr_t unaliased;

    if ((expr_type = m_expr->GetExprType()) && (unaliased = expr_type->GetUnaliased())) {
        m_string_expr.Reset(new AstString(
            unaliased->ToString(false),
            m_location
        ));
    } else {
        m_string_expr.Reset(new AstString(
            BuiltinTypes::UNDEFINED->ToString(),
            m_location
        ));
    }

    m_string_expr->Visit(visitor, mod);
#endif
}

std::unique_ptr<Buildable> AstTypeOfExpression::Build(AstVisitor *visitor, Module *mod)
{
    auto chunk = BytecodeUtil::Make<BytecodeChunk>();
    chunk->Append(AstPrototypeSpecification::Build(visitor, mod));

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    chunk->Append(m_type_ref->Build(visitor, mod));
#else
    AssertThrow(m_string_expr != nullptr);
    chunk->Append(m_string_expr->Build(visitor, mod));
#endif

    return chunk;
}

void AstTypeOfExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AstPrototypeSpecification::Optimize(visitor, mod);
    
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->Optimize(visitor, mod);
#else
    AssertThrow(m_string_expr != nullptr);
    m_string_expr->Optimize(visitor, mod);
#endif
}

RC<AstStatement> AstTypeOfExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstTypeOfExpression::GetExprType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetExprType();
#else
    return BuiltinTypes::STRING;
#endif
}

SymbolTypePtr_t AstTypeOfExpression::GetHeldType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetHeldType();
#else
    return AstExpression::GetHeldType();
#endif
}

const AstExpression *AstTypeOfExpression::GetValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetValueOf();
#else
    AssertThrow(m_string_expr != nullptr);

    return m_string_expr->GetValueOf();
#endif
}

const AstExpression *AstTypeOfExpression::GetDeepValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_ref != nullptr);

    return m_type_ref->GetDeepValueOf();
#else
    AssertThrow(m_string_expr != nullptr);

    return m_string_expr->GetDeepValueOf();
#endif
}

} // namespace hyperion::compiler
