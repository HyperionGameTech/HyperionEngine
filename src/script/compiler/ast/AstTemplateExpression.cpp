#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>
#include <util/utf8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstTemplateExpression::AstTemplateExpression(
    const std::shared_ptr<AstExpression> &expr,
    const std::vector<std::shared_ptr<AstParameter>> &generic_params,
    const std::shared_ptr<AstPrototypeSpecification> &return_type_specification,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_generic_params(generic_params),
      m_return_type_specification(return_type_specification)
{
}

void AstTemplateExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    // visit params before expression to make declarations
    // for things that may be used in the expression
    for (auto &generic_param : m_generic_params) {
        AssertThrow(generic_param != nullptr);

        // gross, but we gotta do this our else on first pass,
        // these will just refer to the wrong memory
        if (generic_param->GetDefaultValue() == nullptr) {
            generic_param->SetDefaultValue(std::shared_ptr<AstNil>(
                new AstNil(generic_param->GetLocation())
            ));
        }

        generic_param->Visit(visitor, mod);
    }

    // added because i made creation of this object happen in parser
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    AssertThrow(m_expr->GetExprType() != nullptr);

    std::vector<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.reserve(m_generic_params.size() + 1); // anotha one
    
    SymbolTypePtr_t expr_return_type;

    // if return type has been specified - visit it and check to see
    // if it's compatible with the expression
    if (m_return_type_specification != nullptr) {
        m_return_type_specification->Visit(visitor, mod);

        AssertThrow(m_return_type_specification->GetHeldType() != nullptr);

        SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
            visitor,
            mod,
            m_return_type_specification->GetHeldType(),
            m_expr->GetExprType(),
            m_return_type_specification->GetLocation()
        );

        expr_return_type = m_return_type_specification->GetHeldType();
    } else {
        expr_return_type = m_expr->GetExprType();
    }

    generic_param_types.push_back({
        "@return",
        expr_return_type
    });

    for (size_t i = 0; i < m_generic_params.size(); i++) {
        const std::shared_ptr<AstParameter> &param = m_generic_params[i];
        AssertThrow(param != nullptr);

        generic_param_types.push_back(GenericInstanceTypeInfo::Arg {
            param->GetName(),
            param->GetIdentifier()->GetSymbolType(),
            nullptr
        });
    }

    m_symbol_type = SymbolType::GenericInstance(
        BuiltinTypes::GENERIC_VARIABLE_TYPE,
        GenericInstanceTypeInfo {
            generic_param_types
        }
    );
}

std::unique_ptr<Buildable> AstTemplateExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    for (auto &generic_param : m_generic_params) {
        AssertThrow(generic_param != nullptr);
        chunk->Append(generic_param->Build(visitor, mod));
    }

    // attempt at using the template expression directly...
    AssertThrow(m_expr != nullptr);
    chunk->Append(m_expr->Build(visitor, mod));

    return std::move(chunk);
}

void AstTemplateExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTemplateExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstTemplateExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstTemplateExpression::GetExprType() const
{
    // AssertThrow(m_expr != nullptr);
    // return m_expr->GetExprType();
    AssertThrow(m_symbol_type != nullptr);
    return m_symbol_type;
}

} // namespace hyperion::compiler
