#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>

#include <iostream>

namespace hyperion::compiler {

AstMemberCallExpression::AstMemberCallExpression(
    const String& fieldName,
    const RC<AstExpression>& target,
    const RC<AstArgumentList>& arguments,
    const SourceLocation& location)
    : AstMember(fieldName, target, location),
      m_arguments(arguments)
{
}

void AstMemberCallExpression::Visit(AstVisitor* visitor, Module* mod)
{
    AstMember::Visit(visitor, mod);

    RC<AstExpression> selfTarget = CloneAstNode(m_target);

    RC<AstArgument> selfArg(new AstArgument(
        selfTarget,
        false,
        false,
        false,
        false,
        "self",
        selfTarget->GetLocation()));

    const SizeType numArguments = m_arguments != nullptr
        ? m_arguments->GetArguments().Size() + 1
        : 1;

    Array<RC<AstArgument>> argsWithSelf;
    argsWithSelf.Reserve(numArguments);
    argsWithSelf.PushBack(selfArg);

    if (m_arguments != nullptr)
    {
        for (const RC<AstArgument>& arg : m_arguments->GetArguments())
        {
            argsWithSelf.PushBack(arg);
        }
    }

    // visit each argument
    for (const RC<AstArgument>& arg : argsWithSelf)
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
        m_returnType = BuiltinTypes::ANY;
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
            m_returnType = BuiltinTypes::UNDEFINED;

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
        for (const RC<AstArgument>& arg : m_substitutedArgs)
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

    Assert(m_target != nullptr);
    chunk->Append(m_target->Build(visitor, mod));

    Assert(m_targetType != nullptr);

    // now we have to call the function. we pop the first arg from
    // m_substituted args because we have already pushed self to stack
    m_substitutedArgs.PopFront();

    // location of 'self' var
    const int targetStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // use above as self arg so PUSH
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add instruction to store on stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    // increment stack size for 'self' var
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    uint16 numArgsToPop = 0;

    if (m_substitutedArgs.Any())
    {
        // build arguments
        chunk->Append(Compiler::BuildArgumentsStart(
            visitor,
            mod,
            m_substitutedArgs,
            numArgsToPop));

        int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // load our target from the stack into the current register (where we put it before)
        auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
        instrLoadOffset->GetBuilder().Load(rp).Local().ByOffset(stackSize - targetStackLocation);
        chunk->Append(std::move(instrLoadOffset));
    }

    const HashCode::ValueType hash = HashCode::GetHashCode(m_fieldName.Data()).Value();

    chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));

    chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_fieldName));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        nullptr,         // no target -- handled above
        numArgsToPop + 1 // +1 for self
        ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        numArgsToPop + 1 // pops self off stack as well
        ));

    return chunk;
}

void AstMemberCallExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    AstMember::Optimize(visitor, mod);

    for (const RC<AstArgument>& arg : m_substitutedArgs)
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

RC<AstStatement> AstMemberCallExpression::Clone() const
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

SymbolTypeRef AstMemberCallExpression::GetExprType() const
{
    Assert(m_returnType != nullptr);
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

} // namespace hyperion::compiler
