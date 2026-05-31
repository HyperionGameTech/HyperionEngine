#include <Lang/Compiler/Ast/AstTryCatch.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion {

AstTryCatch::AstTryCatch(const RC<AstBlock>& tryBlock,
    const RC<AstBlock>& catchBlock,
    const SourceLocation& location)
    : AstStatement(location),
      m_tryBlock(tryBlock),
      m_catchBlock(catchBlock)
{
}

void AstTryCatch::Visit(AstVisitor* visitor, Module* mod)
{
    // accept the try block
    m_tryBlock->Visit(visitor, mod);
    // accept the catch block
    m_catchBlock->Visit(visitor, mod);
}

UniquePtr<Buildable> AstTryCatch::Build(AstVisitor* visitor, Module* mod)
{
    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // the label to jump to the very end
    LabelId endLabel = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(endLabel);

    // the label to jump to the catch-block
    LabelId catchLabel = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(catchLabel);

    { // send the instruction to enter the try-block
        auto instrBeginTry = BytecodeUtil::Make<TryCatchInfo>();
        instrBeginTry->catchLabelId = catchLabel;
        chunk->Append(std::move(instrBeginTry));
    }

    // try block increases stack size to hold the data about the catch block
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    // build the try-block
    chunk->Append(m_tryBlock->Build(visitor, mod));

    { // send the instruction to end the try-block
        auto instrEndTry = BytecodeUtil::Make<RawOperation<>>();
        instrEndTry->opcode = END_TRY;
        chunk->Append(std::move(instrEndTry));
    }

    // decrease stack size for the try block
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    // jump to the end, as to not execute the catch-block
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, endLabel));

    // set the label's position to where the catch-block would be
    chunk->Append(BytecodeUtil::Make<LabelMarker>(catchLabel));

    // exception was thrown, pop all local variables from the try-block
    chunk->Append(Compiler::PopStack(visitor, m_tryBlock->NumLocals()));

    // build the catch-block
    chunk->Append(m_catchBlock->Build(visitor, mod));

    chunk->Append(BytecodeUtil::Make<LabelMarker>(endLabel));

    return chunk;
}

void AstTryCatch::Optimize(AstVisitor* visitor, Module* mod)
{
    // optimize the try block
    m_tryBlock->Optimize(visitor, mod);
    // optimize the catch block
    m_catchBlock->Optimize(visitor, mod);
}

RC<AstStatement> AstTryCatch::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
