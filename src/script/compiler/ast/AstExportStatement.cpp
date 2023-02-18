#include <script/compiler/ast/AstExportStatement.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstExportStatement::AstExportStatement(
    const RC<AstStatement> &stmt,
    const SourceLocation &location
) : AstStatement(location),
    m_stmt(stmt)
{
}

void AstExportStatement::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_stmt != nullptr);
    m_stmt->Visit(visitor, mod);

    if (!mod->IsInGlobalScope()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_export_outside_global,
            m_location
        ));

        return;
    }

    m_exported_symbol_name = m_stmt->GetName();

    if (m_exported_symbol_name == AstStatement::unnamed) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_export_invalid_name,
            m_location
        ));

        return;
    }

    // TODO: ensure thing not already exported globally (or in module?)
}

std::unique_ptr<Buildable> AstExportStatement::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_stmt != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // Bake the value in, let it do what it needs
    chunk->Append(m_stmt->Build(visitor, mod));

    // Get active register (where data that was set should be held)
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // add EXPORT instruction
    chunk->Append(BytecodeUtil::Make<SymbolExport>(rp, m_exported_symbol_name));
    
    return chunk;
}

void AstExportStatement::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_stmt != nullptr);
    m_stmt->Optimize(visitor, mod);
}

RC<AstStatement> AstExportStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
