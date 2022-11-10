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

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
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

    m_block.reset(new AstBlock({}, m_location));

    mod->m_scopes.Open(Scope(
        SCOPE_TYPE_GENERIC_INSTANTIATION, 0
    ));

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, mod);
    }

    // visit the expression
    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

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
        const SymbolTypePtr_t symbol_type = value_of != nullptr ? value_of->GetExprType() : nullptr;

        if (value_of && symbol_type != nullptr && symbol_type->IsGeneric()) {


        // if (const auto *template_expr = dynamic_cast<const AstTemplateExpression *>(value_of)) {
            FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
                visitor, mod, /*template_expr->GetExprType()*/ symbol_type, m_generic_args, m_location
            );

            const auto args_substituted = substituted.second;

            // there can be more because of varargs
            if (args_substituted.size() + 1 >= symbol_type->GetGenericInstanceInfo().m_generic_args.size()) {//template_expr->GetGenericParameters().size()) {
                for (size_t i = 1; i <  symbol_type->GetGenericInstanceInfo().m_generic_args.size(); i++) {
                    AssertThrow(args_substituted[i - 1]->GetExpr() != nullptr);

                    if (args_substituted[i - 1]->GetExpr()->GetExprType() != BuiltinTypes::UNDEFINED) {
                        std::shared_ptr<AstVariableDeclaration> param_override(new AstVariableDeclaration(
                            symbol_type->GetGenericInstanceInfo().m_generic_args[i].m_name,//template_expr->GetGenericParameters()[i]->GetName(),
                            nullptr,
                            CloneAstNode(args_substituted[i - 1]->GetExpr()),
                            {},
                            IdentifierFlags::FLAG_CONST,
                            args_substituted[i - 1]->GetLocation()
                        ));

                        m_block->AddChild(param_override);
                    }
                }

                // AssertThrow(template_expr->GetInnerExpression() != nullptr);

                m_inner_expr = CloneAstNode(value_of);//template_expr->GetInnerExpression());

                m_block->AddChild(m_inner_expr);
                m_block->Visit(visitor, mod);

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

    AssertThrow(m_block != nullptr);
    chunk->Append(m_block->Build(visitor, mod));

    return std::move(chunk);
}

void AstTemplateInstantiation::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_block != nullptr);
    m_block->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateInstantiation::IsTrue() const
{
    AssertThrow(m_inner_expr != nullptr);

    return m_inner_expr->IsTrue();
}

bool AstTemplateInstantiation::MayHaveSideEffects() const
{
    AssertThrow(m_inner_expr != nullptr);

    for (auto &arg : m_generic_args) {
        AssertThrow(arg != nullptr);

        if (arg->MayHaveSideEffects()) {
            return true;
        }
    }

    return m_inner_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstTemplateInstantiation::GetExprType() const
{
    if (m_inner_expr != nullptr) {
        return m_inner_expr->GetExprType();
    }

    return BuiltinTypes::UNDEFINED;
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
        AssertThrow(m_inner_expr.get() != this);

        return m_inner_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
