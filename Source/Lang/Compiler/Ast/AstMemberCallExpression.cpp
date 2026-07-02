#include <Lang/Compiler/Ast/AstMemberCallExpression.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/HashCode.hpp>

#include <iostream>

#include <AstMemberCallExpression.generated.inl>

namespace Hyperion {

AstMemberCallExpression::AstMemberCallExpression(
    const String& fieldName,
    const SharedPtr<AstExpression>& target,
    const SharedPtr<AstArgumentList>& arguments,
    const SourceLocation& location)
    : AstMember(fieldName, target, location),
      m_arguments(arguments),
      m_returnType(nullptr)
{
}

void AstMemberCallExpression::Visit(AstVisitor* visitor, Module* mod)
{
    AstMember::Visit(visitor, mod);

    SharedPtr<AstExpression> selfTarget = CloneAstNode(m_target);

    SharedPtr<AstArgument> selfArg(new AstArgument(
        selfTarget,
        /* isSplat */ false,
        /* isNamed */ false,
        /* isPassByRef */ false,
        /* isPassConst */ false,
        "self",
        selfTarget->GetLocation()));

    const size_t numArguments = m_arguments != nullptr
        ? m_arguments->GetArguments().Size() + 1
        : 1;

    Array<SharedPtr<AstArgument>> argsWithSelf;
    argsWithSelf.Reserve(numArguments);
    argsWithSelf.PushBack(selfArg);

    if (m_arguments != nullptr)
    {
        for (const SharedPtr<AstArgument>& arg : m_arguments->GetArguments())
        {
            argsWithSelf.PushBack(arg);
        }
    }

    // visit each argument
    for (const SharedPtr<AstArgument>& arg : argsWithSelf)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    if (m_symbolType->IsAnyType())
    {
        m_returnType = BuiltinTypes::s_anyType;
        m_substitutedArgs = argsWithSelf; // NOTE: do not clone because we don't need to visit again.
    }
    else
    {
        bool substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
            visitor,
            mod,
            m_symbolType,
            argsWithSelf,
            m_location,
            m_returnType,
            m_substitutedArgs);

        if (!substituted)
        {
            m_returnType = BuiltinTypes::s_errorType;

            // not a function type
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_function,
                m_location,
                m_symbolType->ToString(true)));

            return;
        }

        Assert(m_returnType != nullptr);

        // visit each argument (again, substituted)
        for (const SharedPtr<AstArgument>& arg : m_substitutedArgs)
        {
            Assert(arg != nullptr);

            if (arg->IsPlaceholderArgument())
            {
                continue;
            }

            arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }

        SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
            visitor,
            mod,
            m_symbolType,
            m_substitutedArgs,
            m_location);
    }

    // should never be empty; self is needed
    if (m_substitutedArgs.Empty())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));
    }
}

UniquePtr<Buildable> AstMemberCallExpression::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin member call: " + ToString()));

    // now we have to call the function. we pop the first arg from
    // m_substituted args because we have already pushed self to stack
    m_substitutedArgs.PopFront();

    uint16 numArgsToPop = 0;

    int selfStackLocation = -1;

    { // self load
        chunk->Append(BytecodeUtil::Make<Comment>("Loading target object for method call"));
        Assert(m_target != nullptr);
        chunk->Append(m_target->Build(visitor, mod));

        const uint8 selfArgRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        chunk->Append(BytecodeUtil::Make<Comment>("Pushing 'self' argument to stack (register " + String::ToString(selfArgRegister) + ")"));
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(selfArgRegister);
        chunk->Append(std::move(instrPush));

        selfStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    if (m_substitutedArgs.Any())
    {
        // build arguments
        chunk->Append(Compiler::BuildArgumentsStart(
            visitor,
            mod,
            m_substitutedArgs,
            numArgsToPop));
    }

    Assert(selfStackLocation != -1);

    { // load self back into register
        chunk->Append(BytecodeUtil::Make<Comment>("Loading 'self' argument back into register (stack location " + String::ToString(selfStackLocation) + ")"));

        const uint8 currentRegister = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
        instrLoadOffset->opcode = LOAD_OFFSET;
        instrLoadOffset->Accept<uint8>(currentRegister);
        instrLoadOffset->Accept<uint16>(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - selfStackLocation);
        chunk->Append(std::move(instrLoadOffset));
    }

    {
        const HashCode::ValueType hash = HashCode::GetHashCode(m_fieldName.Data()).Value();

        chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_fieldName + " (method call) - hash: " + String::ToString(hash)));
        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
    }

    // num args for call may be > numArgsToPop.
    // (placeholder args don't count towards stack size and are handled outside of this node)
    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        nullptr,                             // no target -- handled above
        uint16(m_substitutedArgs.Size()) + 1 // +1 for self.
        ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        numArgsToPop + 1 // pops self off stack as well
        ));

    chunk->Append(BytecodeUtil::Make<Comment>("End member call: " + ToString()));

    return chunk;
}

void AstMemberCallExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    AstMember::Optimize(visitor, mod);

    for (const SharedPtr<AstArgument>& arg : m_substitutedArgs)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        arg->Optimize(visitor, mod);
    }

    // TODO: check if the member being accessed is constant and can
    // be optimized
}

SharedPtr<AstStatement> AstMemberCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstMemberCallExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstMemberCallExpression::MayHaveSideEffects() const
{
    return true;
}

const SymbolType* AstMemberCallExpression::GetExprType() const
{
    return m_returnType;
}

const AstExpression* AstMemberCallExpression::GetValueOf() const
{
    // if (m_symbolType != nullptr && m_symbolType->GetDefaultValue() != nullptr) {
    //     return m_symbolType->GetDefaultValue()->GetValueOf();
    // }

    return AstExpression::GetValueOf();
}

const AstExpression* AstMemberCallExpression::GetDeepValueOf() const
{
    // if (m_symbolType != nullptr && m_symbolType->GetDefaultValue() != nullptr) {
    //     return m_symbolType->GetDefaultValue()->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

AstExpression* AstMemberCallExpression::GetTarget() const
{
    if (m_target != nullptr)
    {
        // if (auto *nestedTarget = m_target->GetTarget()) {
        //     return nestedTarget;
        // }

        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

String AstMemberCallExpression::ToString() const
{
    String result = (m_target ? m_target->ToString() : "<null>") + "." + m_fieldName + "(";

    if (m_arguments && !m_arguments->GetArguments().Empty())
    {
        for (size_t i = 0; i < m_arguments->GetArguments().Size(); ++i)
        {
            if (i > 0)
                result += ", ";
            const auto& arg = m_arguments->GetArguments()[i];
            result += arg ? arg->ToString() : "<null>";
        }
    }

    result += ")";
    return result;
}

} // namespace Hyperion
