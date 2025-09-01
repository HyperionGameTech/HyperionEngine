#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstNewExpression::AstNewExpression(
    const RC<AstTypeSpecifier>& proto,
    const RC<AstArgumentList>& argList,
    bool enableConstructorCall,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_proto(proto),
      m_argList(argList),
      m_enableConstructorCall(enableConstructorCall)
{
}

void AstNewExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(m_proto != nullptr);
    m_proto->Visit(visitor, mod);

    if (m_argList != nullptr)
    {
        Assert(m_enableConstructorCall, "Args provided for non-constructor call new expr");
    }

    auto* valueOf = m_proto->GetDeepValueOf();
    Assert(valueOf != nullptr);

    m_instanceType = BuiltinTypes::UNDEFINED;

    SymbolTypeRef exprType = valueOf->GetExprType();
    Assert(exprType != nullptr);
    exprType = exprType->GetUnaliased();

    if (SymbolTypeRef heldType = valueOf->GetHeldType())
    {
        m_instanceType = heldType->GetUnaliased();
        m_objectValue = m_proto->GetDefaultValue(); // may be nullptr
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

    // if (m_instanceType != nullptr && m_instanceType->IsProxyClass()) {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_proxy_class_cannot_be_constructed,
    //         m_location
    //     ));

    //     return;
    // }

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
                        CloneAstNode(m_proto),
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
                        CloneAstNode(m_proto),
                        nullptr, // no args
                        false,   // do not enable constructor call
                        m_location)),
                    IdentifierFlags::FLAG_CONST,
                    m_location));

                m_constructorBlock->AddChild(tempVarDecl);

                m_constructorCall.Reset(new AstTernaryExpression(
                    RC<AstHasExpression>(new AstHasExpression(
                        RC<AstVariable>(new AstVariable(
                            tempVarName,
                            m_location)),
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

#if HYP_SCRIPT_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE
    // does not currently work in templates
    // e.g `new X` where `X` is `String` as a template argument, attempts to
    // construct the object rather than baking in
    if (m_objectValue != nullptr && m_instanceType->GetTypeClass() == TYPE_BUILTIN)
    {
        chunk->Append(m_objectValue->Build(visitor, mod));
    }
    else
#endif
    {
        Assert(m_proto != nullptr);
        chunk->Append(m_proto->Build(visitor, mod));

        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instrNew = BytecodeUtil::Make<RawOperation<>>();
        instrNew->opcode = NEW;
        instrNew->Accept<uint8>(rp); // dst (overwrite proto)
        instrNew->Accept<uint8>(rp); // src (holds proto)
        chunk->Append(std::move(instrNew));
    }

    return chunk;
}

void AstNewExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_constructorBlock != nullptr)
    {
        m_constructorBlock->Optimize(visitor, mod);

        return;
    }

    Assert(m_proto != nullptr);
    m_proto->Optimize(visitor, mod);

    if (m_objectValue != nullptr)
    {
        m_objectValue->Optimize(visitor, mod);
    }
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

    if (m_objectValue != nullptr)
    {
        return m_objectValue->IsTrue();
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
    if (m_constructorCall != nullptr)
    {
        return m_constructorCall->GetExprType();
    }

    Assert(m_instanceType != nullptr);
    return m_instanceType;
    // Assert(m_typeExpr != nullptr);
    // Assert(m_typeExpr->GetSpecifiedType() != nullptr);

    // return m_typeExpr->GetSpecifiedType();
}

AstExpression* AstNewExpression::GetTarget() const
{
    if (m_constructorCall != nullptr)
    {
        return m_constructorCall->GetTarget();
    }

    return m_objectValue.Get();
}

} // namespace hyperion::compiler
