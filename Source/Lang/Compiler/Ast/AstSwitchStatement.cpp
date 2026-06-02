#include <Lang/Compiler/Ast/AstSwitchStatement.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Scope.hpp>
#include <Lang/Compiler/CompilerError.hpp>
#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Ast/AstName.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/InstructionStream.hpp>

#include <Lang/Instructions.hpp>

#include <Core/Containers/FlatSet.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

AstSwitchStatement::AstSwitchStatement(
    const RC<AstExpression>& expression,
    const Array<CaseClause>& clauses,
    const SourceLocation& location)
    : AstStatement(location),
      m_expression(expression),
      m_clauses(clauses),
      m_numPops(0)
{
}

void AstSwitchStatement::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_expression != nullptr);

    mod->scopeTree.Open(SCOPE_TYPE_SWITCH);

    m_expression->Visit(visitor, mod);

    bool hasDefault = false;
    int defaultValueIndex = -1;
    FlatSet<HashCode> seenCaseValues;

    for (int i = 0; i < int(m_clauses.Size()); i++)
    {
        auto& clause = m_clauses[i];

        if (clause.m_isDefault)
        {
            if (hasDefault)
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_duplicate_default,
                    clause.m_block->GetLocation()));
            }

            hasDefault = true;
            defaultValueIndex = i;
        }
        else
        {
            Assert(clause.m_value != nullptr);

            clause.m_value->Visit(visitor, mod);

            ConstantValue constVal = clause.m_value->GetConstantValue();

            if (constVal.IsValid())
            {
                HashCode hc = constVal.GetHashCode();

                if (seenCaseValues.Contains(hc))
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_switch_duplicate_case,
                        clause.m_value->GetLocation()));
                }
                else
                {
                    seenCaseValues.Insert(hc);
                }
            }
            else if (auto* nameExpr = dynamic_cast<AstName*>(clause.m_value.Get()))
            {
                HashCode hc = HashCode::GetHashCode(nameExpr->GetValue());

                if (seenCaseValues.Contains(hc))
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_switch_duplicate_case,
                        clause.m_value->GetLocation()));
                }
                else
                {
                    seenCaseValues.Insert(hc);
                }
            }
            else if (auto* strExpr = dynamic_cast<AstString*>(clause.m_value.Get()))
            {
                HashCode hc = HashCode::GetHashCode(strExpr->GetValue());

                if (seenCaseValues.Contains(hc))
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_switch_duplicate_case,
                        clause.m_value->GetLocation()));
                }
                else
                {
                    seenCaseValues.Insert(hc);
                }
            }
            else
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_switch_case_requires_constant,
                    clause.m_value->GetLocation()));
            }
        }

        Assert(clause.m_block != nullptr);
        clause.m_block->Visit(visitor, mod);
    }

    Scope& thisScope = mod->scopeTree.Top();
    m_numPops = thisScope.identifierTable.CountUsedVariables();

    mod->scopeTree.Close();
}

UniquePtr<Buildable> AstSwitchStatement::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin switch statement"));

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_SWITCH);

    LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(SwitchBreakLabel));
    chunk->TakeOwnershipOfLabel(breakLabel);

    LabelId defaultLabel;

    int defaultIndex = -1;
    int nonDefaultCount = 0;

    for (int i = 0; i < int(m_clauses.Size()); i++)
    {
        if (m_clauses[i].m_isDefault)
        {
            defaultIndex = i;
        }
        else
        {
            nonDefaultCount++;
        }
    }

    Array<LabelId> caseLabels;
    caseLabels.Resize(m_clauses.Size());

    for (int i = 0; i < int(m_clauses.Size()); i++)
    {
        LabelId label = contextGuard->NewLabel();
        chunk->TakeOwnershipOfLabel(label);
        caseLabels[i] = label;
    }

    if (defaultIndex >= 0)
    {
        defaultLabel = caseLabels[defaultIndex];
    }
    else
    {
        defaultLabel = breakLabel;
    }

    uint8 exprReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    chunk->Append(m_expression->Build(visitor, mod));

    for (int i = 0; i < int(m_clauses.Size()); i++)
    {
        auto& clause = m_clauses[i];

        if (clause.m_isDefault)
        {
            continue;
        }

        chunk->Append(BytecodeUtil::Make<Comment>("case " + clause.m_value->ToString()));

        uint8 prevReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        chunk->Append(clause.m_value->Build(visitor, mod));

        uint8 caseReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMP, exprReg, caseReg));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, caseLabels[i]));

        while (visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister() > prevReg)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        }
    }

    if (defaultIndex < 0)
    {
        chunk->Append(BytecodeUtil::Make<Comment>("no case matched, break"));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, breakLabel));
    }
    else
    {
        chunk->Append(BytecodeUtil::Make<Comment>("no case matched, jump to default"));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, defaultLabel));
    }

    for (int i = 0; i < int(m_clauses.Size()); i++)
    {
        chunk->Append(BytecodeUtil::Make<LabelMarker>(caseLabels[i]));
        chunk->Append(m_clauses[i].m_block->Build(visitor, mod));
    }

    chunk->Append(BytecodeUtil::Make<LabelMarker>(breakLabel));

    chunk->Append(BytecodeUtil::Make<Comment>("End switch statement"));

    return chunk;
}

void AstSwitchStatement::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_expression != nullptr)
    {
        m_expression->Optimize(visitor, mod);
    }

    for (auto& clause : m_clauses)
    {
        if (clause.m_value != nullptr)
        {
            clause.m_value->Optimize(visitor, mod);
        }

        if (clause.m_block != nullptr)
        {
            clause.m_block->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstSwitchStatement::Clone() const
{
    return CloneImpl();
}

RC<AstSwitchStatement> AstSwitchStatement::CloneImpl() const
{
    Array<CaseClause> clonedClauses;
    clonedClauses.Resize(m_clauses.Size());

    for (const auto& clause : m_clauses)
    {
        CaseClause clonedClause;
        clonedClause.m_value = CloneAstNode(clause.m_value);
        clonedClause.m_block = CloneAstNode(clause.m_block);
        clonedClause.m_isDefault = clause.m_isDefault;
        clonedClauses.PushBack(clonedClause);
    }

    return RC<AstSwitchStatement>(new AstSwitchStatement(
        CloneAstNode(m_expression),
        clonedClauses,
        m_location));
}

} // namespace Hyperion
