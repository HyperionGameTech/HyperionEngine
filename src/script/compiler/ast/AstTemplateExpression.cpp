#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstTemplateExpression::AstTemplateExpression(
    const RC<AstExpression> &expr,
    const Array<RC<AstParameter>> &generic_params,
    const RC<AstPrototypeSpecification> &return_type_specification,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_generic_params(generic_params),
    m_return_type_specification(return_type_specification)
{
}

void AstTemplateExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_is_visited);
    m_is_visited = true;

    m_symbol_type = BuiltinTypes::UNDEFINED;

    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG));

    m_block.Reset(new AstBlock(m_location));


    // visit params before expression to make declarations
    // for things that may be used in the expression

    {
        for (auto &generic_param : m_generic_params) {
            AssertThrow(generic_param != nullptr);

            generic_param->SetIsGenericParam(true);

            if (generic_param->GetPrototypeSpecification() == nullptr) {
                generic_param->SetPrototypeSpecification(RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstVariable>(new AstVariable(
                        BuiltinTypes::ANY->GetName(),
                        m_location
                    )),
                    m_location
                )));
            }

            // gross, but we gotta do this our else on first pass,
            // these will just refer to the wrong memory
            if (generic_param->GetDefaultValue() == nullptr) {
                generic_param->SetDefaultValue(RC<AstNil>(new AstNil(
                    m_location
                )));
            }

            m_block->AddChild(generic_param);
        }
    }

    // added because i made creation of this object happen in parser
    AssertThrow(m_expr != nullptr);
    // m_expr->Visit(visitor, mod);

    m_block->AddChild(m_expr);
    m_block->Visit(visitor, mod);

    AssertThrow(m_expr->GetExprType() != nullptr);

    Array<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.Reserve(m_generic_params.Size() + 1); // anotha one
    
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

    generic_param_types.PushBack({
        "@return",
        expr_return_type
    });

    for (SizeType i = 0; i < m_generic_params.Size(); i++) {
        const RC<AstParameter> &param = m_generic_params[i];
        AssertThrow(param != nullptr);

        SymbolTypePtr_t param_type = BuiltinTypes::UNDEFINED;

        if (param->GetIdentifier() && param->GetIdentifier()->GetSymbolType()) {
            param_type = param->GetIdentifier()->GetSymbolType();
        }

        generic_param_types.PushBack(GenericInstanceTypeInfo::Arg {
            param->GetName(),
            param_type,
            param->GetDefaultValue()
        });
    }

    m_symbol_type = SymbolType::GenericInstance(
        BuiltinTypes::GENERIC_VARIABLE_TYPE,
        GenericInstanceTypeInfo {
            generic_param_types
        }
    );

    AssertThrow(m_symbol_type != nullptr);

    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstTemplateExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    if (m_block == nullptr) {
        return nullptr;
    }

    return m_block->Build(visitor, mod);
}

void AstTemplateExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    // AssertThrow(m_expr != nullptr);
    // m_expr->Optimize(visitor, mod);

    if (m_block == nullptr) {
        return;
    }

    m_block->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateExpression::Clone() const
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
    AssertThrow(m_is_visited);
    AssertThrow(m_symbol_type != nullptr);
    return m_symbol_type;
}

const AstExpression *AstTemplateExpression::GetValueOf() const
{
    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateExpression::GetDeepValueOf() const
{
    // if (m_expr != nullptr) {
    //     AssertThrow(m_expr.Get() != this);

    //     return m_expr->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

const AstExpression *AstTemplateExpression::GetHeldGenericExpr() const
{
    return m_expr.Get();
}

} // namespace hyperion::compiler
