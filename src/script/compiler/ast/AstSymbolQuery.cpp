#include <script/compiler/ast/AstSymbolQuery.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>

namespace hyperion::compiler {

AstSymbolQuery::AstSymbolQuery(
    const std::string &command_name,
    const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_command_name(command_name),
      m_expr(expr)
{
}

void AstSymbolQuery::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_symbol_type = BuiltinTypes::UNDEFINED;

    if (m_command_name == "inspect_type") {
        SymbolTypePtr_t expr_type = m_expr->GetExprType();
        AssertThrow(expr_type != nullptr);

        m_string_result_value = std::shared_ptr<AstString>(new AstString(
            expr_type->GetName(),
            m_location
        ));
    } else if (m_command_name == "fields") {
        SymbolTypePtr_t expr_type = m_expr->GetExprType();
        AssertThrow(expr_type != nullptr);

        //if (AstTypeObject *as_type_object = dynamic_cast<AstTypeObject*>(m_expr.get())) {
           // if (const SymbolTypePtr_t &held_type = as_type_object->GetHeldType()) {
                std::vector<std::shared_ptr<AstExpression>> field_names;

                for (const auto &member : expr_type->GetMembers()) {
                    field_names.push_back(std::shared_ptr<AstString>(new AstString(
                        std::get<0>(member),
                        m_location
                    )));
                }

                m_array_result_value = std::shared_ptr<AstArrayExpression>(new AstArrayExpression(
                    field_names,
                    m_location
                ));

                m_array_result_value->Visit(visitor, mod);
                
           // }
        //}
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_invalid_symbol_query,
            m_location,
            m_command_name
        ));
    }
}

std::unique_ptr<Buildable> AstSymbolQuery::Build(AstVisitor *visitor, Module *mod)
{
    if (AstExpression *value_of = const_cast<AstExpression*>(GetValueOf())) {
        return value_of->Build(visitor, mod);
    }

    return nullptr;
}

void AstSymbolQuery::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstSymbolQuery::Clone() const
{
    return CloneImpl();
}

Tribool AstSymbolQuery::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstSymbolQuery::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstSymbolQuery::GetExprType() const
{
    if (m_string_result_value != nullptr) {
        return m_string_result_value->GetExprType();
    } else if (m_array_result_value != nullptr) {
        return m_array_result_value->GetExprType();
    }

    return BuiltinTypes::UNDEFINED;
}

const AstExpression *AstSymbolQuery::GetValueOf() const
{
    if (m_string_result_value != nullptr) {
        return m_string_result_value.get();
    } else if (m_array_result_value != nullptr) {
        return m_array_result_value.get();
    }

    return this;
}

} // namespace hyperion::compiler
