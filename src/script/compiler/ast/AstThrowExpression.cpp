#include <script/compiler/ast/AstThrowExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion {

AstThrowExpression::AstThrowExpression(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr)
{
}

void AstThrowExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    m_expr->Visit(visitor, mod);
}

UniquePtr<Buildable> AstThrowExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(m_expr->Build(visitor, mod));

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // compile in the instruction to check if it has the member
        auto instrThrow = BytecodeUtil::Make<RawOperation<>>();
        instrThrow->opcode = THROW;
        instrThrow->Accept<uint8>(rp);
        chunk->Append(std::move(instrThrow));
    }

    return chunk;
}

void AstThrowExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);

    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstThrowExpression::Clone() const
{
    return CloneImpl();
}

const SymbolType* AstThrowExpression::GetExprType() const
{
    Assert(m_expr != nullptr);

    return m_expr->GetExprType();
}

Tribool AstThrowExpression::IsTrue() const
{
    Assert(m_expr != nullptr);

    return m_expr->IsTrue();
}

bool AstThrowExpression::MayHaveSideEffects() const
{
    return true;
}

} // namespace hyperion
