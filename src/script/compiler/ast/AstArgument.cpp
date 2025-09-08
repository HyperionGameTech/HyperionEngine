#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion {

AstArgument::AstArgument(
    const RC<AstExpression>& expr,
    bool isSplat,
    bool isNamed,
    bool isPassByRef,
    bool isPassConst,
    const String& name,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_isSplat(isSplat),
      m_isNamed(isNamed),
      m_isPassByRef(isPassByRef),
      m_isPassConst(isPassConst),
      m_name(name)
{
}

void AstArgument::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_isVisited);
    m_isVisited = true;

    if (m_isSplat)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_implemented,
            m_location,
            "splat-expressions"));
    }

    Assert(m_expr != nullptr);

    bool passByRefScope = false;
    bool passConstScope = false;

    if (IsPassConst())
    {
        Scope& scope = mod->scopeTree.Open(SCOPE_TYPE_NORMAL, CONST_VARIABLE_FLAG);

        passConstScope = true;
    }

    if (IsPassByRef())
    {
        if (m_expr->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE)
        {
            Scope& scope = mod->scopeTree.Open(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG);

            passByRefScope = true;
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR, Msg_cannot_create_reference, m_location));
        }
    }

    m_expr->Visit(visitor, mod);

    if (passByRefScope)
    {
        mod->scopeTree.Close();
    }

    if (passConstScope)
    {
        mod->scopeTree.Close();
    }
}

UniquePtr<Buildable> AstArgument::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    Assert(m_isVisited);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(m_expr->Build(visitor, mod));

    // if (IsPassByRef()) {
    //     // TODO! Ensure in the analyzing stage that we are passing in something we can take the reference of.
    //     uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     chunk->Append(BytecodeUtil::Make<LoadRef>(rp, rp));
    // }

    return chunk;
}

void AstArgument::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstArgument::Clone() const
{
    return CloneImpl();
}

bool AstArgument::IsLiteral() const
{
    Assert(m_expr != nullptr);
    return m_expr->IsLiteral();
}

Tribool AstArgument::IsTrue() const
{
    Assert(m_expr != nullptr);
    return m_expr->IsTrue();
}

bool AstArgument::MayHaveSideEffects() const
{
    Assert(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypeRef AstArgument::GetExprType() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression* AstArgument::GetValueOf() const
{
    return m_expr.Get();
}

const AstExpression* AstArgument::GetDeepValueOf() const
{
    if (!m_expr)
    {
        return nullptr;
    }

    return m_expr->GetDeepValueOf();
}

const String& AstArgument::GetName() const
{
    return m_name;
}

String AstArgument::ToString() const
{
    String result;

    if (m_isPassConst)
        result += "const ";
    if (m_isPassByRef)
        result += "ref ";
    if (m_isSplat)
        result += "...";

    if (m_isNamed && !m_name.Empty())
    {
        result += m_name + ": ";
    }

    result += m_expr ? m_expr->ToString() : "<null>";

    return result;
}

} // namespace hyperion
