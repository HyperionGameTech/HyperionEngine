#include <Lang/Compiler/Ast/AstForEachLoop.hpp>
#include <Lang/Compiler/Ast/AstUndefined.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/Instruction.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>
#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <AstForEachLoop.generated.inl>

namespace Hyperion {

AstForEachLoop::AstForEachLoop(
    const Handle<AstVariableDeclaration>& varDecl,
    const Handle<AstExpression>& iterable,
    const Handle<AstBlock>& block,
    const SourceLocation& location)
    : AstStatement(location),
      m_varDecl(varDecl),
      m_iterable(iterable),
      m_block(block),
      m_numLocals(0)
{
}

void AstForEachLoop::Visit(AstVisitor* visitor, Module* mod)
{
    m_iterable->Visit(visitor, mod);

    const SymbolType* iterableType = m_iterable->GetExprType();
    Assert(iterableType != nullptr);
    iterableType = iterableType->GetUnaliased();

    if (!iterableType->IsOrHasBase(*BuiltinTypes::s_arrayBaseType))
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_invalid_iterable_type,
            m_location,
            iterableType->ToString()));

        return;
    }

    Assert(iterableType->GetGenericInstanceInfo().m_genericArgs.Size() == 1);

    const SymbolType* elementType = iterableType->GetGenericInstanceInfo().m_genericArgs.Front().m_type;
    Assert(elementType != nullptr);

    SymbolType* selfAliasType = SymbolType::Alias("SelfType", { iterableType });
    selfAliasType->Register(visitor->GetCompilationUnit());

    mod->scopeTree.Open(SCOPE_TYPE_LOOP);
    {
        ScopeGuard aliasScope(mod, SCOPE_TYPE_NORMAL);
        aliasScope->identifierTable.AddSymbolType(selfAliasType);

        const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
            visitor,
            mod,
            elementType,
            m_location);

        Assert(resolvedType != nullptr);
        resolvedType->Register(visitor->GetCompilationUnit());
        elementType = resolvedType;
    }

    if (m_varDecl->GetTypeSpecifier() == nullptr)
    {
        m_varDecl->SetTypeSpecifier(MakeHandle<AstTypeSpecifier>(
            MakeHandle<AstTypeRef>(elementType, m_varDecl->GetLocation()),
            m_varDecl->GetLocation()));
    }

    m_varDecl->Visit(visitor, mod);

    if (m_varDecl->GetIdentifier() != nullptr)
    {
        m_varDecl->GetIdentifier()->SetSymbolType(elementType);
    }

    mod->scopeTree.Open(SCOPE_TYPE_LOOP);

    m_block->Visit(visitor, mod);

    m_numLocals = mod->scopeTree.Top().identifierTable.CountUsedVariables();

    mod->scopeTree.Close();

    mod->scopeTree.Close();
}

UniquePtr<Buildable> AstForEachLoop::Build(AstVisitor* visitor, Module* mod)
{
    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_LOOP);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin for-each loop: " + ToString()));

    chunk->Append(m_iterable->Build(visitor, mod));
    uint8 arrayReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(arrayReg);
        chunk->Append(std::move(instrPush));

        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    const int arrayStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - 1;

    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    uint8 sizeFuncReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    {
        HashCode::ValueType sizeHash = HashCode::GetHashCode("Size").Value();

        auto instrLoadSize = BytecodeUtil::Make<StorageOperation>();
        instrLoadSize->GetBuilder().Load(sizeFuncReg).Member(arrayReg).ByHash(sizeHash);
        chunk->Append(std::move(instrLoadSize));
    }

    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(arrayReg);
        chunk->Append(std::move(instrPush));

        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    {
        auto instrCall = BytecodeUtil::Make<RawOperation<>>();
        instrCall->opcode = CALL;
        instrCall->Accept<uint8>(sizeFuncReg);
        instrCall->Accept<uint8>(1);
        chunk->Append(std::move(instrCall));
    }

    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    chunk->Append(Compiler::PopStack(visitor, 1));

    visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(0);
        chunk->Append(std::move(instrPush));

        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    const int sizeStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - 1;

    chunk->Append(m_varDecl->Build(visitor, mod));
    const int varStackLocation = m_varDecl->GetIdentifier()->GetStackLocation();

    {
        uint8 initReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        chunk->Append(BytecodeUtil::Make<ConstI32>(initReg, 0));

        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(initReg);
        chunk->Append(std::move(instrPush));

        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    const int indexStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - 1;

    LabelId topLabel = contextGuard->NewLabel(HYP_NAME(LoopTopLabel));
    chunk->TakeOwnershipOfLabel(topLabel);

    LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(LoopBreakLabel));
    chunk->TakeOwnershipOfLabel(breakLabel);

    LabelId continueLabel = contextGuard->NewLabel(HYP_NAME(LoopContinueLabel));
    chunk->TakeOwnershipOfLabel(continueLabel);

    chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

    {
        uint8 indexReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        int idxOffset = stackSize - indexStackLocation;
        auto instrLoadIdx = BytecodeUtil::Make<StorageOperation>();
        instrLoadIdx->GetBuilder().Load(indexReg).Local().ByOffset(idxOffset);
        chunk->Append(std::move(instrLoadIdx));

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        uint8 sizeReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        int sizeOffset = stackSize - sizeStackLocation;
        auto instrLoadSize = BytecodeUtil::Make<StorageOperation>();
        instrLoadSize->GetBuilder().Load(sizeReg).Local().ByOffset(sizeOffset);
        chunk->Append(std::move(instrLoadSize));

        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMP, indexReg, sizeReg));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JGE, breakLabel));

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        uint8 loadArrayReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        int arrOffset = stackSize - arrayStackLocation;
        auto instrLoadArr = BytecodeUtil::Make<StorageOperation>();
        instrLoadArr->GetBuilder().Load(loadArrayReg).Local().ByOffset(arrOffset);
        chunk->Append(std::move(instrLoadArr));

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        uint8 elemReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        {
            auto instrLoad = BytecodeUtil::Make<RawOperation<>>();
            instrLoad->opcode = LOAD_ARRAYIDX;
            instrLoad->Accept<uint8>(elemReg);
            instrLoad->Accept<uint8>(loadArrayReg);
            instrLoad->Accept<uint8>(indexReg);
            chunk->Append(std::move(instrLoad));
        }

        {
            int varOffset = stackSize - varStackLocation;

            auto instrStore = BytecodeUtil::Make<StorageOperation>();
            instrStore->GetBuilder().Store(elemReg).Local().ByOffset(varOffset);
            chunk->Append(std::move(instrStore));
        }

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
    }

    chunk->Append(m_block->Build(visitor, mod));

    chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

    for (int i = 0; i < m_numLocals; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    chunk->Append(Compiler::PopStack(visitor, m_numLocals));

    {
        uint8 indexReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int offset = stackSize - indexStackLocation;

        auto instrLoadIdx = BytecodeUtil::Make<StorageOperation>();
        instrLoadIdx->GetBuilder().Load(indexReg).Local().ByOffset(offset);
        chunk->Append(std::move(instrLoadIdx));

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        uint8 oneReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        chunk->Append(BytecodeUtil::Make<ConstI32>(oneReg, 1));

        auto instrAdd = BytecodeUtil::Make<RawOperation<>>();
        instrAdd->opcode = ADD;
        instrAdd->Accept<uint8>(indexReg);
        instrAdd->Accept<uint8>(oneReg);
        instrAdd->Accept<uint8>(indexReg);
        chunk->Append(std::move(instrAdd));

        auto instrStoreIdx = BytecodeUtil::Make<StorageOperation>();
        instrStoreIdx->GetBuilder().Store(indexReg).Local().ByOffset(offset);
        chunk->Append(std::move(instrStoreIdx));

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
    }

    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, topLabel));

    chunk->Append(BytecodeUtil::Make<LabelMarker>(breakLabel));

    for (int i = 0; i < 4; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        chunk->Append(Compiler::PopStack(visitor, 1));
    }

    return chunk;
}

void AstForEachLoop::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_iterable != nullptr)
    {
        m_iterable->Optimize(visitor, mod);
    }

    if (m_varDecl != nullptr)
    {
        m_varDecl->Optimize(visitor, mod);
    }

    if (m_block != nullptr)
    {
        m_block->Optimize(visitor, mod);
    }
}

Handle<AstStatement> AstForEachLoop::Clone() const
{
    return CloneImpl();
}

String AstForEachLoop::ToString() const
{
    String result = "for (";

    if (m_varDecl)
    {
        result += m_varDecl->ToString();
    }

    result += " in ";

    if (m_iterable)
    {
        result += m_iterable->ToString();
    }

    result += ") { ... }";

    return result;
}

} // namespace Hyperion
