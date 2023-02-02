#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeName.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

AstTypeOfExpression::AstTypeOfExpression(
    const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location
) : AstPrototypeSpecification(nullptr, location),
    m_expr(expr)
{
}

void AstTypeOfExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_symbol_type = BuiltinTypes::UNDEFINED;

#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    if (const auto &expr_type = m_expr->GetExprType()) {
        mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, 0));

        SymbolTypePtr_t internal_type = SymbolType::Alias(
            "__typeof",
            { expr_type }
        );

        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(internal_type);

        AstPrototypeSpecification::m_proto.reset(new AstVariable(
            "__typeof",
            m_location
        ));

        AstPrototypeSpecification::Visit(visitor, mod);

        mod->m_scopes.Close();

        // m_type_object = CloneAstNode(expr_type->GetTypeObject());
        AssertThrow(m_type_object != nullptr);

        // m_type_object.reset(new AstTypeObject(
        //     expr_type,
        //     nullptr,
        //     m_location
        // ));

        // m_type_object->Visit(visitor, mod);
    } else {
        m_type_object.reset();
    }

#else
    m_symbol_type = BuiltinTypes::STRING;

    SymbolTypePtr_t expr_type;
    SymbolTypePtr_t unaliased;

    if ((expr_type = m_expr->GetExprType()) && (unaliased = expr_type->GetUnaliased())) {
        m_string_expr.reset(new AstString(
            unaliased->GetName(),
            m_location
        ));
    } else {
        m_string_expr.reset(new AstString(
            BuiltinTypes::UNDEFINED->GetName(),
            m_location
        ));
    }

    m_string_expr->Visit(visitor, mod);
#endif
}

std::unique_ptr<Buildable> AstTypeOfExpression::Build(AstVisitor *visitor, Module *mod)
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_object != nullptr);

    return m_type_object->Build(visitor, mod);
#else
    AssertThrow(m_string_expr != nullptr);
    return m_string_expr->Build(visitor, mod);
#endif
}

void AstTypeOfExpression::Optimize(AstVisitor *visitor, Module *mod)
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_object != nullptr);

    return m_type_object->Optimize(visitor, mod);
#else
    AssertThrow(m_string_expr != nullptr);
    m_string_expr->Optimize(visitor, mod);
#endif
}

Pointer<AstStatement> AstTypeOfExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstTypeOfExpression::GetExprType() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_object != nullptr);

    return m_type_object->GetExprType();
#else
    return BuiltinTypes::STRING;
#endif
}

const AstExpression *AstTypeOfExpression::GetValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_object != nullptr);

    return m_type_object->GetValueOf();
#else
    AssertThrow(m_string_expr != nullptr);

    return m_string_expr->GetValueOf();
#endif
}

const AstExpression *AstTypeOfExpression::GetDeepValueOf() const
{
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    AssertThrow(m_type_object != nullptr);

    return m_type_object->GetDeepValueOf();
#else
    AssertThrow(m_string_expr != nullptr);

    return m_string_expr->GetDeepValueOf();
#endif
}

} // namespace hyperion::compiler
