#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <limits>
#include <iostream>

namespace hyperion::compiler {

AstCallExpression::AstCallExpression(
    const std::shared_ptr<AstExpression> &target,
    const std::vector<std::shared_ptr<AstArgument>> &args,
    bool insert_self,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_args(args),
      m_insert_self(insert_self),
      m_return_type(BuiltinTypes::UNDEFINED),
      m_is_method_call(false)
{
}

void AstCallExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);

    if (m_insert_self) {
        // if the target is a member expression,
        // place it as 'self' argument to the call
        if (auto *target_mem = dynamic_cast<const AstMember *>(m_target.get())) {
            m_is_method_call = true;

            AssertThrow(target_mem->GetTarget() != nullptr);

            std::shared_ptr<AstArgument> self_arg(new AstArgument(
                target_mem->GetTarget(),
                false,
                true,
                "self",
                target_mem->GetTarget()->GetLocation()
            ));
            
            // insert at front
            m_args.insert(m_args.begin(), self_arg);
        }
    }

    // allow unboxing
    SymbolTypePtr_t unboxed_type = target_type;

    if (target_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        if (target_type->IsBoxedType()) {
            unboxed_type = target_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
        }
    }

    AssertThrow(unboxed_type != nullptr);

    if (SymbolTypePtr_t call_member_type = unboxed_type->FindPrototypeMember("$invoke")) {
        m_is_method_call = true;

        // closure objects have a self parameter for the '$invoke' call.
        std::shared_ptr<AstArgument> self_arg(new AstArgument(
            m_target,
            false,
            false,
            "__closure_self",
            m_target->GetLocation()
        ));
        
        // insert at front
        m_args.insert(m_args.begin(), self_arg);

        m_target.reset(new AstMember(
            "$invoke",
            CloneAstNode(m_target),
            m_location
        ));
        
        AssertThrow(m_target != nullptr);
        m_target->Visit(visitor, mod);

        unboxed_type = call_member_type;
        AssertThrow(unboxed_type != nullptr);
    }

    // visit each argument
    for (auto &arg : m_args) {
        AssertThrow(arg != nullptr);

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    DebugLog(
        LogType::Debug,
        "Substitute args for type %s   %s\n",
        unboxed_type->GetName().c_str(),
        typeid(*m_target).name()
    );

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor,
        mod,
        unboxed_type,
        m_args,
        m_location
    );

    if (substituted.first != nullptr) {
        m_return_type = substituted.first;
        // change args to be newly ordered vector
        m_args = substituted.second;
    } else {
        // not a function type
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_function,
            m_location,
            target_type->GetName()
        ));
    }
}

std::unique_ptr<Buildable> AstCallExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build arguments
    chunk->Append(Compiler::BuildArgumentsStart(
        visitor,
        mod,
        m_args//args_sorted
    ));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        m_target,
        (uint8_t)m_args.size()
    ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_args.size()
    ));

    return std::move(chunk);
}

void AstCallExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);

    // optimize each argument
    for (auto &arg : m_args) {
        if (arg != nullptr) {
            arg->Optimize(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }
    }
}

Pointer<AstStatement> AstCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstCallExpression::IsTrue() const
{
    // cannot deduce if return value is true
    return Tribool::Indeterminate();
}

bool AstCallExpression::MayHaveSideEffects() const
{
    // assume a function call has side effects
    // maybe we could detect this later
    return true;
}

SymbolTypePtr_t AstCallExpression::GetExprType() const
{
    AssertThrow(m_return_type != nullptr);
    return m_return_type;
}

} // namespace hyperion::compiler
