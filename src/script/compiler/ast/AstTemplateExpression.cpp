#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
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

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(true);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG));

    m_block.Reset(new AstBlock(m_location));

    // visit params before expression to make declarations
    // for things that may be used in the expression

    m_generic_param_type_objects.Reserve(m_generic_params.Size());

    for (auto &generic_param : m_generic_params) {
        AssertThrow(generic_param != nullptr);

        SymbolTypePtr_t base_type = BuiltinTypes::CLASS_TYPE;

        if (generic_param->IsVariadic()) {
            base_type = BuiltinTypes::VAR_ARGS;
        }

        SymbolTypePtr_t generic_param_type = SymbolType::GenericParameter(
            generic_param->GetName(),
            base_type
        );

        RC<AstTypeObject> generic_param_type_object(new AstTypeObject(
            generic_param_type,
            base_type,
            m_location
        ));

        generic_param_type->SetTypeObject(generic_param_type_object);

        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(generic_param_type);

        // when visited, will register the SymbolType
        m_block->AddChild(generic_param_type_object);

        m_generic_param_type_objects.PushBack(std::move(generic_param_type_object));

        auto var_decl = RC<AstVariableDeclaration>(new AstVariableDeclaration(
            generic_param->GetName(),
            nullptr,
            RC<AstTypeRef>(new AstTypeRef(
                generic_param_type,
                SourceLocation::eof
            )),
            IdentifierFlags::FLAG_CONST,
            m_location
        ));

        m_block->AddChild(var_decl);
    }
    
    if (m_return_type_specification != nullptr) {
        m_block->AddChild(m_return_type_specification);
    }

    AssertThrow(m_expr != nullptr);

    m_block->AddChild(m_expr);
    m_block->Visit(visitor, mod);

    Array<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.Reserve(m_generic_params.Size() + 1); // anotha one for @return
    
    SymbolTypePtr_t expr_return_type;
    SymbolTypePtr_t explicit_return_type = m_return_type_specification != nullptr
        ? m_return_type_specification->GetHeldType()
        : nullptr;

    // if return type has been specified - visit it and check to see
    // if it's compatible with the expression
    if (m_return_type_specification != nullptr) {
        AssertThrow(explicit_return_type != nullptr);
        
        expr_return_type = explicit_return_type;
    } else {
        expr_return_type = BuiltinTypes::PLACEHOLDER;
    }

    generic_param_types.PushBack({
        "@return",
        expr_return_type
    });

    AssertThrow(m_generic_params.Size() == m_generic_param_type_objects.Size());

    for (SizeType i = 0; i < m_generic_params.Size(); i++) {
        RC<AstParameter> &param = m_generic_params[i];
        RC<AstTypeObject> &param_type_object = m_generic_param_type_objects[i];

        SymbolTypePtr_t param_type = param_type_object->GetHeldType();
        AssertThrow(param_type != nullptr);
        param_type = param_type->GetUnaliased();

        RC<AstExpression> default_value = CloneAstNode(param->GetDefaultValue());

        if (default_value == nullptr) {
            default_value = RC<AstTypeRef>(new AstTypeRef(
                param_type,
                SourceLocation::eof
            ));
        }

        generic_param_types.PushBack(GenericInstanceTypeInfo::Arg {
            param->GetName(),
            std::move(param_type),
            std::move(default_value)
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

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(false);
}

std::unique_ptr<Buildable> AstTemplateExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_is_visited);

    return nullptr; // Uninstantiated generic types are not buildable
}

void AstTemplateExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    // do nothing - instantiated generic expressions will be optimized
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

SymbolTypePtr_t AstTemplateExpression::GetHeldType() const
{
    AssertThrow(m_is_visited);
    AssertThrow(m_expr != nullptr);

    return m_expr->GetHeldType();
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
