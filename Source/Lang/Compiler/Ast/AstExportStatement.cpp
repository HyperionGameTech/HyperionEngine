#include <Lang/Compiler/Ast/AstExportStatement.hpp>
#include <Lang/Compiler/Optimizer.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion {

AstExportStatement::AstExportStatement(
    const RC<AstStatement>& stmt,
    const SourceLocation& location)
    : AstStatement(location),
      m_stmt(stmt)
{
}

void AstExportStatement::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_stmt != nullptr);
    m_stmt->Visit(visitor, mod);

    if (!mod->IsInGlobalScope())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_export_outside_global,
            m_location));

        return;
    }

    m_exportedSymbolName = m_stmt->GetName();

    if (m_exportedSymbolName == AstStatement::s_unnamed)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_export_invalid_name,
            m_location));

        return;
    }

    // TODO: ensure thing not already exported globally (or in module?)
}

UniquePtr<Buildable> AstExportStatement::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_stmt != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // Bake the value in, let it do what it needs
    chunk->Append(m_stmt->Build(visitor, mod));

    // Get active register (where data that was set should be held)
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // add EXPORT instruction
    chunk->Append(BytecodeUtil::Make<SymbolExport>(rp, m_exportedSymbolName));

    return chunk;
}

void AstExportStatement::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_stmt != nullptr);
    m_stmt->Optimize(visitor, mod);
}

RC<AstStatement> AstExportStatement::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
