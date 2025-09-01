#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>
#include <limits>

namespace hyperion::compiler {

UniquePtr<Buildable> Compiler::BuildArgumentsStart(
    AstVisitor* visitor,
    Module* mod,
    const Array<RC<AstArgument>>& args)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8 rp;

    // push a copy of each argument to the stack
    for (auto& arg : args)
    {
        Assert(arg != nullptr);

        // build in current module (not mod)
        chunk->Append(arg->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // now that it's loaded into the register, make a copy
        // add instruction to store on stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    return chunk;
}

UniquePtr<Buildable> Compiler::BuildArgumentsEnd(
    AstVisitor* visitor,
    Module* mod,
    uint8 nargs)
{
    // the reason we decrement the compiler's record of the stack size directly after
    // is because the function body will actually handle the management of the stack size,
    // so that the parameters are actually local variables to the function body.
    for (uint8 i = 0; i < nargs; i++)
    {
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // pop arguments from stack
    return Compiler::PopStack(visitor, nargs);
}

UniquePtr<Buildable> Compiler::BuildCall(
    AstVisitor* visitor,
    Module* mod,
    const RC<AstExpression>& target,
    uint8 nargs)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // if no target provided, do not build it in
    if (target != nullptr)
    {
        chunk->Append(target->Build(visitor, mod));
    }

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instrCall = BytecodeUtil::Make<RawOperation<>>();
    instrCall->opcode = CALL;
    instrCall->Accept<uint8>(rp);
    instrCall->Accept<uint8>(nargs);
    chunk->Append(std::move(instrCall));

    return chunk;
}

UniquePtr<Buildable> Compiler::LoadMemberFromHash(AstVisitor* visitor, Module* mod, HashCode::ValueType hash)
{
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instrLoadMemHash = BytecodeUtil::Make<StorageOperation>();
    instrLoadMemHash->GetBuilder().Load(rp).Member(rp).ByHash(hash);
    return instrLoadMemHash;
}

UniquePtr<Buildable> Compiler::StoreMemberFromHash(AstVisitor* visitor, Module* mod, HashCode::ValueType hash)
{
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instrMovMemHash = BytecodeUtil::Make<StorageOperation>();
    instrMovMemHash->GetBuilder().Store(rp - 1).Member(rp).ByHash(hash);
    return instrMovMemHash;
}

UniquePtr<Buildable> Compiler::CreateConditional(
    AstVisitor* visitor,
    Module* mod,
    AstStatement* cond,
    AstStatement* thenPart,
    AstStatement* elsePart)
{
    Assert(cond != nullptr);
    Assert(thenPart != nullptr);

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8 rp;

    LabelId endLabel = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(endLabel);

    LabelId elseLabel = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(elseLabel);

    // get current register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    // build the conditional
    chunk->Append(cond->Build(visitor, mod));

    // compare the conditional to 0
    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

    // get current register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    {
        LabelId labelId;

        if (elsePart != nullptr)
        {
            labelId = elseLabel;
        }
        else
        {
            labelId = endLabel;
        }

        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, labelId));
    }

    // enter the block
    chunk->Append(thenPart->Build(visitor, mod));

    if (elsePart != nullptr)
    {
        // jump to the very end now that we've accepted the if-block
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, endLabel));

        // set the label's position to where the else-block would be
        chunk->Append(BytecodeUtil::Make<LabelMarker>(elseLabel));
        // build the else-block
        chunk->Append(elsePart->Build(visitor, mod));
    }

    // set the label's position to after the block,
    // so we can skip it if the condition is false
    chunk->Append(BytecodeUtil::Make<LabelMarker>(endLabel));

    return chunk;
}

UniquePtr<Buildable> Compiler::LoadLeftThenRight(AstVisitor* visitor, Module* mod, Compiler::ExprInfo info)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // load left-hand side into register 0
    chunk->Append(info.left->Build(visitor, mod));

    // right side has not been optimized away
    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    if (info.right != nullptr)
    {
        // load right-hand side into register 1
        chunk->Append(info.right->Build(visitor, mod));
    }

    return chunk;
}

UniquePtr<Buildable> Compiler::LoadRightThenLeft(AstVisitor* visitor, Module* mod, Compiler::ExprInfo info)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8 rp;

    // load right-hand side into register 0
    chunk->Append(info.right->Build(visitor, mod));
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    bool leftSideEffects = info.left->MayHaveSideEffects();

    // if left is a function call, we have to move rhs to the stack!
    // otherwise, the function call will overwrite what's in register 0.
    int stackSizeBefore = 0;
    if (leftSideEffects)
    {
        { // store value of the right hand side on the stack
            auto instrPush = BytecodeUtil::Make<RawOperation<>>();
            instrPush->opcode = PUSH;
            instrPush->Accept<uint8>(rp);
            chunk->Append(std::move(instrPush));
        }

        stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }
    else
    {
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    }

    // load left-hand side into register 1
    chunk->Append(info.left->Build(visitor, mod));

    if (leftSideEffects)
    {
        // now, we increase register usage to load rhs from the stack into register 1.
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        // get register position
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        // load from stack
        int stackSizeAfter = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int diff = stackSizeAfter - stackSizeBefore;
        Assert(diff == 1);

        {
            auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
            instrLoadOffset->opcode = LOAD_OFFSET;
            instrLoadOffset->Accept<uint8>(rp);
            instrLoadOffset->Accept<uint16>(diff);
            chunk->Append(std::move(instrLoadOffset));
        }

        {
            auto instrPop = BytecodeUtil::Make<RawOperation<>>();
            instrPop->opcode = POP;
            chunk->Append(std::move(instrPop));
        }

        // decrement stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

UniquePtr<Buildable> Compiler::LoadLeftAndStore(
    AstVisitor* visitor,
    Module* mod,
    Compiler::ExprInfo info)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8 rp;

    // load left-hand side into register 0
    chunk->Append(info.left->Build(visitor, mod));
    // get register position
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // store value of lhs on the stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // increment stack size
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    // do NOT increase register usage (yet)
    // load right-hand side into register 0, overwriting previous lhs
    chunk->Append(info.right->Build(visitor, mod));

    // now, we increase register usage to load lhs from the stack into register 1.
    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    // get register position
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // load from stack
    int stackSizeAfter = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    int diff = stackSizeAfter - stackSizeBefore;

    Assert(diff == 1);

    {
        auto instrLoadOffset = BytecodeUtil::Make<RawOperation<>>();
        instrLoadOffset->opcode = LOAD_OFFSET;
        instrLoadOffset->Accept<uint8>(rp);
        instrLoadOffset->Accept<uint16>(diff);
        chunk->Append(std::move(instrLoadOffset));
    }

    { // pop from stack
        auto instrPop = BytecodeUtil::Make<RawOperation<>>();
        instrPop->opcode = POP;
        chunk->Append(std::move(instrPop));
    }

    // decrement stack size
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    return chunk;
}

UniquePtr<Buildable> Compiler::BuildBinOp(
    uint8 opcode,
    AstVisitor* visitor,
    Module* mod,
    Compiler::ExprInfo info)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AstBinaryExpression* leftAsBinop = dynamic_cast<AstBinaryExpression*>(info.left);
    AstBinaryExpression* rightAsBinop = dynamic_cast<AstBinaryExpression*>(info.right);

    uint8 rp;

    if (leftAsBinop == nullptr && rightAsBinop != nullptr)
    {
        // if the right hand side is a binary operation,
        // we should build in the rhs first in order to
        // transverse the parse tree.
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto rawOperation = BytecodeUtil::Make<RawOperation<>>();
        rawOperation->opcode = opcode;
        rawOperation->Accept<uint8>(rp);     // lhs
        rawOperation->Accept<uint8>(rp - 1); // rhs
        rawOperation->Accept<uint8>(rp - 1); // dst

        chunk->Append(std::move(rawOperation));
    }
    else if (info.right != nullptr && info.right->MayHaveSideEffects())
    {
        // lhs must be temporary stored on the stack,
        // to avoid the rhs overwriting it.
        if (info.left->MayHaveSideEffects())
        {
            chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto rawOperation = BytecodeUtil::Make<RawOperation<>>();
            rawOperation->opcode = opcode;
            rawOperation->Accept<uint8>(rp);     // lhs
            rawOperation->Accept<uint8>(rp - 1); // rhs
            rawOperation->Accept<uint8>(rp - 1); // dst

            chunk->Append(std::move(rawOperation));
        }
        else
        {
            // left  doesn't have side effects,
            // so just evaluate right without storing the lhs.
            chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto rawOperation = BytecodeUtil::Make<RawOperation<>>();
            rawOperation->opcode = opcode;
            rawOperation->Accept<uint8>(rp);     // lhs
            rawOperation->Accept<uint8>(rp - 1); // rhs
            rawOperation->Accept<uint8>(rp - 1); // dst

            chunk->Append(std::move(rawOperation));
        }
    }
    else
    {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));

        if (info.right != nullptr)
        {
            // perform operation
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto rawOperation = BytecodeUtil::Make<RawOperation<>>();
            rawOperation->opcode = opcode;
            rawOperation->Accept<uint8>(rp - 1); // lhs
            rawOperation->Accept<uint8>(rp);     // rhs
            rawOperation->Accept<uint8>(rp - 1); // dst

            chunk->Append(std::move(rawOperation));
        }
    }

    return chunk;
}

UniquePtr<Buildable> Compiler::PopStack(AstVisitor* visitor, int amt)
{
    if (amt == 1)
    {
        auto instrPop = BytecodeUtil::Make<RawOperation<>>();
        instrPop->opcode = POP;

        return instrPop;
    }

    for (int i = 0; i < amt;)
    {
        int j = 0;

        while (j < MathUtil::MaxSafeValue<uint16>() && i < amt)
        {
            j++, i++;
        }

        if (j > 0)
        {
            Assert(j <= MathUtil::MaxSafeValue<uint16>());

            auto instrSubSp = BytecodeUtil::Make<RawOperation<>>();
            instrSubSp->opcode = SUB_SP;
            instrSubSp->Accept<uint16>(j);

            return instrSubSp;
        }
    }

    return nullptr;
}

Compiler::Compiler(AstIterator* astIterator, CompilationUnit* compilationUnit)
    : AstVisitor(astIterator, compilationUnit)
{
}

Compiler::Compiler(const Compiler& other)
    : AstVisitor(other.m_astIterator, other.m_compilationUnit)
{
}

UniquePtr<BytecodeChunk> Compiler::Compile()
{
    UniquePtr<BytecodeChunk> chunk(new BytecodeChunk);

    Module* mod = m_compilationUnit->GetCurrentModule();
    Assert(mod != nullptr);

    while (m_astIterator->HasNext())
    {
        auto next = m_astIterator->Next();
        Assert(next != nullptr);

        chunk->Append(next->Build(this, mod));
    }

    return chunk;
}

} // namespace hyperion::compiler
