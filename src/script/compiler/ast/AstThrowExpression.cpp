#include <script/compiler/ast/AstThrowExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <iostream>

namespace hyperion::compiler {

AstThrowExpression::AstThrowExpression(
    const RC<AstExpression> &expr,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr)
{
}

void AstThrowExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstThrowExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(m_expr->Build(visitor, mod));

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // compile in the instruction to check if it has the member
        auto instr_throw = BytecodeUtil::Make<RawOperation<>>();
        instr_throw->opcode = THROW;
        instr_throw->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_throw));
    }

    return chunk;
}

void AstThrowExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstThrowExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstThrowExpression::GetExprType() const
{
    AssertThrow(m_expr != nullptr);

    return m_expr->GetExprType();
}

Tribool AstThrowExpression::IsTrue() const
{
    AssertThrow(m_expr != nullptr);

    return m_expr->IsTrue();
}

bool AstThrowExpression::MayHaveSideEffects() const
{
    return true;
}

} // namespace hyperion::compiler
