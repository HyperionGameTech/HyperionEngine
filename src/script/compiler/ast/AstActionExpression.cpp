#include <script/compiler/ast/AstActionExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/Hasher.hpp>
#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <limits>
#include <iostream>

namespace hyperion {
namespace compiler {

AstActionExpression::AstActionExpression(
    const std::vector<std::shared_ptr<AstArgument>> &actions,
    const std::shared_ptr<AstExpression> &target,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_actions(actions),
      m_target(target),
      m_return_type(BuiltinTypes::ANY),
      m_is_method_call(false),
      m_member_found(-1)
{
}

void AstActionExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(!m_actions.empty());
    //m_action->Visit(visitor, mod);

    AssertThrow(m_target != nullptr);
    //m_target->Visit(visitor, mod);

    /*std::shared_ptr<AstArgument> self_arg(new AstArgument(
        m_target,
        true,
        "self",
        m_target->GetLocation()
    ));
    
    // insert self to front
    m_args.insert(m_args.begin(), self_arg);*/
    
    /*std::shared_ptr<AstArgument> action_arg((new AstArgument(
        m_action,
        false,
        "",
        SourceLocation::eof
    )));

    m_args.push_back(action_arg);*/

    std::shared_ptr<AstArgument> self_arg((new AstArgument(
        m_target,
        false,
        "",
        SourceLocation::eof
    )));

    m_actions.insert(
        m_actions.begin(),
        self_arg
    );

    // add events::call_action call
    m_expr = visitor->GetCompilationUnit()->GetAstNodeBuilder()
        .Module("events")
        .Function("call_action")
        .Call(m_actions);

    /*m_expr = std::shared_ptr<AstCallExpression>(new AstCallExpression(
        m_expr,
        m_actions,
        false, // no 'self' - do not pass in object.events,
               // instead pass object (see above)
        m_location
    ));*/

    AssertThrow(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_target->GetSymbolType();
    AssertThrow(target_type != nullptr);

    if (target_type != BuiltinTypes::ANY) {
        if (SymbolTypePtr_t member_type = target_type->FindMember("$events")) {
            m_member_found = 1;

            // see if there is an action handler with this key.
            // if it is not found, do not worry about it,
            // it will be resolved at runtime by returning null.
            // however, if it is found, type-check the other arguments.

            // TODO: iterate through the items on the type, find any matches,
            // and attempt to substitute function arguments with the second item in the array
            // (the callback/handler)
            if (member_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
                const SymbolTypePtr_t base = member_type->GetBaseType();
                AssertThrow(base != nullptr);

                if (base == BuiltinTypes::ARRAY) {
                    // iterate through array items
                    // each array item should be a 2d array itself
                    const auto &generic_args = member_type->GetGenericInstanceInfo().m_generic_args;
                }
            }

        } else {
            m_member_found = 0;
        }
    } else {
        // if it is ANY type do not bother checking,
        // we will be resolved at runtime.
        m_member_found = -1;
    }
}

std::unique_ptr<Buildable> AstActionExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
    // re-build in the target. actions return their target after call
    //m_target->Build(visitor, mod);
}

void AstActionExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);

   /* // optimize each argument
    for (auto &arg : m_args) {
        if (arg != nullptr) {
            arg->Optimize(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }
    }*/
}

Pointer<AstStatement> AstActionExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstActionExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstActionExpression::MayHaveSideEffects() const
{
    // assume a function call has side effects
    // maybe we could detect this later
    return m_member_found != 0;
}

SymbolTypePtr_t AstActionExpression::GetSymbolType() const
{
    AssertThrow(m_expr != nullptr);
    AssertThrow(m_expr->GetSymbolType() != nullptr);

    return m_expr->GetSymbolType();
}

} // namespace compiler
} // namespace hyperion
