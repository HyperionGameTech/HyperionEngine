#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion {

AstNewExpression::AstNewExpression(
    const RC<AstTypeSpecifier>& typeSpec,
    const RC<AstArgumentList>& argList,
    bool enableConstructorCall,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_typeSpec(typeSpec),
      m_argList(argList),
      m_enableConstructorCall(enableConstructorCall)
{
}

void AstNewExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(m_typeSpec != nullptr);
    m_typeSpec->Visit(visitor, mod);

    if (m_argList != nullptr)
    {
        Assert(m_enableConstructorCall, "Args provided for non-constructor call new expr");
    }

    auto* valueOf = m_typeSpec->GetDeepValueOf();
    Assert(valueOf != nullptr);

    m_instanceType = BuiltinTypes::s_errorType;

    SymbolTypeRef exprType = valueOf->GetExprType();
    Assert(exprType != nullptr);
    exprType = exprType->GetUnaliased();

    if (SymbolTypeRef heldType = valueOf->GetHeldType())
    {
        m_instanceType = heldType->GetUnaliased();
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            exprType->ToString()));

        return;
    }

    if (!m_instanceType->IsObject())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_instantiate_non_object,
            m_location,
            m_instanceType->ToString()));

        return;
    }

    if (m_enableConstructorCall)
    {
        static constexpr const char* constructMethodName = "$construct";
        static constexpr const char* tempVarName = "__$tempNewTarget";

        const bool isAny = m_instanceType->IsAnyType();
        const bool hasConstructMember = m_instanceType->FindMember(constructMethodName) != nullptr;

        if (isAny || hasConstructMember)
        {
            m_constructorBlock.Reset(new AstBlock(m_location));

            if (hasConstructMember)
            {
                m_constructorCall.Reset(new AstMemberCallExpression(
                    constructMethodName,
                    RC<AstNewExpression>(new AstNewExpression(
                        CloneAstNode(m_typeSpec),
                        nullptr, // no args
                        false,   // do not enable constructor call
                        m_location)),
                    m_argList,
                    m_location));
            }
            else
            {
                // conditionally lookup member with the name $construct and call if it exists.
                // to do this, we need to store a temporary variable holding the left hand side
                // expression

                RC<AstVariableDeclaration> tempVarDecl(new AstVariableDeclaration(
                    tempVarName,
                    nullptr,
                    RC<AstNewExpression>(new AstNewExpression(
                        CloneAstNode(m_typeSpec),
                        nullptr, // no args
                        false,   // do not enable constructor call
                        m_location)),
                    IdentifierFlags::FLAG_CONST,
                    m_location));

                m_constructorBlock->AddChild(tempVarDecl);

                m_constructorCall.Reset(new AstTernaryExpression(
                    RC<AstHasExpression>(new AstHasExpression(
                        RC<AstVariable>(new AstVariable(tempVarName, m_location)),
                        constructMethodName,
                        m_location)),
                    RC<AstMemberCallExpression>(new AstMemberCallExpression(
                        constructMethodName,
                        RC<AstNewExpression>(new AstNewExpression(
                            RC<AstTypeSpecifier>(new AstTypeSpecifier(
                                RC<AstVariable>(new AstVariable(tempVarName, m_location)),
                                m_location)),
                            nullptr, // no args
                            false,   // do not enable constructor call
                            m_location)),
                        m_argList,
                        m_location)),
                    RC<AstVariable>(new AstVariable(tempVarName, m_location)),
                    m_location));
            }

            m_constructorBlock->AddChild(m_constructorCall);

            m_constructorBlock->Visit(visitor, mod);

            // Do not continue analyzing from here, as m_constructorCall contains the new AstNewExpression.
            return;
        }
    }
}

UniquePtr<Buildable> AstNewExpression::Build(AstVisitor* visitor, Module* mod)
{
    if (m_constructorBlock != nullptr)
    {
        return m_constructorBlock->Build(visitor, mod);
    }

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_instanceType != nullptr);
    Assert(m_typeSpec != nullptr);

    chunk->Append(m_typeSpec->Build(visitor, mod));

    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // add LoadClass instruction here
    chunk->Append(BytecodeUtil::Make<LoadClass>(rp, CreateNameFromDynamicString(m_instanceType->GetName())));

    auto instrNew = BytecodeUtil::Make<RawOperation<>>();
    instrNew->opcode = NEW;
    instrNew->Accept<uint8>(rp); // dst (overwrite typeSpec)
    instrNew->Accept<uint8>(rp); // src (holds typeSpec)
    chunk->Append(std::move(instrNew));

    return chunk;
}

void AstNewExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_constructorBlock != nullptr)
    {
        m_constructorBlock->Optimize(visitor, mod);

        return;
    }

    Assert(m_typeSpec != nullptr);
    m_typeSpec->Optimize(visitor, mod);
}

RC<AstStatement> AstNewExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstNewExpression::IsTrue() const
{
    if (m_constructorCall != nullptr)
    {
        return m_constructorCall->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstNewExpression::MayHaveSideEffects() const
{
    if (m_constructorCall != nullptr)
    {
        return m_constructorCall->MayHaveSideEffects();
    }

    return true;
}

SymbolTypeRef AstNewExpression::GetExprType() const
{
    return m_instanceType;
}

AstExpression* AstNewExpression::GetTarget() const
{
    if (m_constructorCall != nullptr)
    {
        return m_constructorCall->GetTarget();
    }

    return nullptr;
}

} // namespace hyperion
