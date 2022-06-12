#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstAliasDeclaration.hpp>
#include <script/compiler/ast/AstMixinDeclaration.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstBlock.hpp>
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

AstTemplateInstantiation::AstTemplateInstantiation(
    const std::shared_ptr<AstExpression> &expr,
    const std::vector<std::shared_ptr<AstArgument>> &generic_args,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_generic_args(generic_args)
{
}

void AstTemplateInstantiation::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    // visit the expression
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    mod->m_scopes.Open(Scope(
        SCOPE_TYPE_NORMAL, 0
    ));

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, mod);
    }

    m_block.reset(new AstBlock({}, m_location));

    const auto &expr_type = m_expr->GetExprType();
    AssertThrow(expr_type != nullptr);

    if (expr_type == BuiltinTypes::ANY) { // 'Any' is not usable for generic inst
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expression_not_generic,
            m_expr->GetLocation(),
            expr_type->GetName()
        ));
    } else {
        // temporarily define all generic parameters.
        const AstExpression *value_of = m_expr->GetValueOf();
        // no need to check if null because if it is the template_expr cast will return null

        m_return_type = BuiltinTypes::UNDEFINED;

        if (const auto *template_expr = dynamic_cast<const AstTemplateExpression *>(value_of)) {
            FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
                visitor, mod, template_expr->GetExprType(), m_generic_args, m_location
            );

            m_return_type = substituted.first;
            const auto args_substituted = substituted.second;

            // there can be more because of varargs
            if (args_substituted.size() >= template_expr->GetGenericParameters().size()) {
                for (size_t i = 0; i <  template_expr->GetGenericParameters().size(); i++) {
                    AssertThrow(args_substituted[i]->GetExpr() != nullptr);

                    if (args_substituted[i]->GetExpr()->GetExprType() != BuiltinTypes::UNDEFINED) {
                        std::shared_ptr<AstVariableDeclaration> param_override(new AstVariableDeclaration(
                            template_expr->GetGenericParameters()[i]->GetName(),
                            nullptr,
                            CloneAstNode(args_substituted[i]->GetExpr()),
                            {},
                            true, // const
                            false, // generic
                            args_substituted[i]->GetLocation()
                        ));

                        //param_override->Visit(visitor, mod);

                        // m_param_overrides.push_back(param_override);
                        m_block->AddChild(param_override);
                    }
                }

                AssertThrow(template_expr->GetInnerExpression() != nullptr);

                m_inner_expr = CloneAstNode(template_expr->GetInnerExpression());
                //m_inner_expr->Visit(visitor, mod);

                m_block->AddChild(m_inner_expr);
                m_block->Visit(visitor, mod);

                // if (const AstTypeObject *inner_expr_as_type_object = dynamic_cast<const AstTypeObject*>(inner_expr->GetValueOf())) {
                //     m_return_type = inner_expr_as_type_object->GetHeldType();
                // }

                // TODO: Cache instantiations so we don't create a new one for every set of arguments
            }
        } else if (const auto *type_object = dynamic_cast<const AstTypeObject *>(value_of)) {
            const auto type_object_type = type_object->GetHeldType() != nullptr
                ? type_object->GetHeldType()
                : type_object->GetExprType();

            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_expression_not_generic,
                m_expr->GetLocation(),
                type_object_type != nullptr
                    ? type_object_type->GetName()
                    : "??"
            ));
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_expression_not_generic,
                m_expr->GetLocation(),
                expr_type->GetName()
            ));
        }
    }

    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstTemplateInstantiation::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // for (auto &it : m_param_overrides/*m_mixin_overrides*/) {
    //     chunk->Append(it->Build(visitor, mod));
    // }

    // // // visit the expression
    // AssertThrow(m_inner_expr != nullptr);
    // chunk->Append(m_inner_expr->Build(visitor, mod));

    AssertThrow(m_block != nullptr);
    chunk->Append(m_block->Build(visitor, mod));


    // pop stack for all mixin values
    //chunk->Append(Compiler::PopStack(visitor, m_mixin_overrides.size()));

    return std::move(chunk);
}

void AstTemplateInstantiation::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    // for (auto &it : m_param_overrides/*m_mixin_overrides*/) {
    //     it->Optimize(visitor, mod);
    // }

    // // optimize the expression
    // AssertThrow(m_inner_expr != nullptr);
    // m_inner_expr->Optimize(visitor, mod);

    AssertThrow(m_block != nullptr);
    m_block->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiation::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstTemplateInstantiation::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstTemplateInstantiation::GetExprType() const
{
    AssertThrow(m_return_type != nullptr);

    return m_return_type;
}

const AstExpression *AstTemplateInstantiation::GetValueOf() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstTemplateInstantiation::GetDeepValueOf() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
