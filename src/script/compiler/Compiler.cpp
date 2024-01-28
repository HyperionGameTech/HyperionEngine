#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <iostream>
#include <limits>

namespace hyperion::compiler {

std::unique_ptr<Buildable> Compiler::BuildArgumentsStart(
    AstVisitor *visitor,
    Module *mod,
    const Array<RC<AstArgument>> &args
)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    UInt8 rp;

    // push a copy of each argument to the stack
    for (auto &arg : args) {
        AssertThrow(arg != nullptr);

        // build in current module (not mod)
        chunk->Append(arg->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // now that it's loaded into the register, make a copy
        // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_push));

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    return chunk;
}

std::unique_ptr<Buildable> Compiler::BuildArgumentsEnd(
    AstVisitor *visitor,
    Module *mod,
    UInt8 nargs
)
{
     // the reason we decrement the compiler's record of the stack size directly after
    // is because the function body will actually handle the management of the stack size,
    // so that the parameters are actually local variables to the function body.
    for (UInt8 i = 0; i < nargs; i++) {
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // pop arguments from stack
    return Compiler::PopStack(visitor, nargs);
}

std::unique_ptr<Buildable> Compiler::BuildCall(
    AstVisitor *visitor,
    Module *mod,
    const RC<AstExpression> &target,
    UInt8 nargs
)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // if no target provided, do not build it in
    if (target != nullptr) {
        chunk->Append(target->Build(visitor, mod));
    }

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    auto instr_call = BytecodeUtil::Make<RawOperation<>>();
    instr_call->opcode = CALL;
    instr_call->Accept<UInt8>(rp);
    instr_call->Accept<UInt8>(nargs);
    chunk->Append(std::move(instr_call));

    return chunk;
}

std::unique_ptr<Buildable> Compiler::LoadMemberFromHash(AstVisitor *visitor, Module *mod, UInt32 hash)
{
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_load_mem_hash = BytecodeUtil::Make<StorageOperation>();
    instr_load_mem_hash->GetBuilder().Load(rp).Member(rp).ByHash(hash);
    return instr_load_mem_hash;
}

std::unique_ptr<Buildable> Compiler::StoreMemberFromHash(AstVisitor *visitor, Module *mod, UInt32 hash)
{
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_mov_mem_hash = BytecodeUtil::Make<StorageOperation>();
    instr_mov_mem_hash->GetBuilder().Store(rp - 1).Member(rp).ByHash(hash);
    return instr_mov_mem_hash;
}

std::unique_ptr<Buildable> Compiler::LoadMemberAtIndex(AstVisitor *visitor, Module *mod, UInt8 index)
{
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_load_mem = BytecodeUtil::Make<RawOperation<>>();
    instr_load_mem->opcode = LOAD_MEM;
    instr_load_mem->Accept<UInt8>(rp); // dst
    instr_load_mem->Accept<UInt8>(rp); // src
    instr_load_mem->Accept<UInt8>(index); // index
    
    return instr_load_mem;
}

std::unique_ptr<Buildable> Compiler::StoreMemberAtIndex(AstVisitor *visitor, Module *mod, UInt8 index)
{
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    auto instr_mov_mem = BytecodeUtil::Make<RawOperation<>>();
    instr_mov_mem->opcode = MOV_MEM;
    instr_mov_mem->Accept<UInt8>(rp); // dst
    instr_mov_mem->Accept<UInt8>(index); // index
    instr_mov_mem->Accept<UInt8>(rp - 1); // src
    
    return instr_mov_mem;
}

std::unique_ptr<Buildable> Compiler::CreateConditional(
    AstVisitor *visitor,
    Module *mod,
    AstStatement *cond,
    AstStatement *then_part,
    AstStatement *else_part)
{
    AssertThrow(cond != nullptr);
    AssertThrow(then_part != nullptr);

    InstructionStreamContextGuard context_guard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT
    );

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    UInt8 rp;

    LabelId end_label = context_guard->NewLabel();
    chunk->TakeOwnershipOfLabel(end_label);

    LabelId else_label = context_guard->NewLabel();
    chunk->TakeOwnershipOfLabel(else_label);

    // get current register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    // build the conditional
    chunk->Append(cond->Build(visitor, mod));

    // compare the conditional to 0
    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

    // get current register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    {
        LabelId label_id;

        if (else_part != nullptr) {
            label_id = else_label;
        } else {
            label_id = end_label;
        }

        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, label_id));
    }

    // enter the block
    chunk->Append(then_part->Build(visitor, mod));

    if (else_part != nullptr) {
        // jump to the very end now that we've accepted the if-block
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, end_label));

        // set the label's position to where the else-block would be
        chunk->Append(BytecodeUtil::Make<LabelMarker>(else_label));
        // build the else-block
        chunk->Append(else_part->Build(visitor, mod));
    }

    // set the label's position to after the block,
    // so we can skip it if the condition is false
    chunk->Append(BytecodeUtil::Make<LabelMarker>(end_label));

    return chunk;
}

std::unique_ptr<Buildable> Compiler::LoadLeftThenRight(AstVisitor *visitor, Module *mod, Compiler::ExprInfo info)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // load left-hand side into register 0
    chunk->Append(info.left->Build(visitor, mod));

    // right side has not been optimized away
    visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    if (info.right != nullptr) {
        // load right-hand side into register 1
        chunk->Append(info.right->Build(visitor, mod));
    }

    return chunk;
}

std::unique_ptr<Buildable> Compiler::LoadRightThenLeft(AstVisitor *visitor, Module *mod, Compiler::ExprInfo info)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    UInt8 rp;

    // load right-hand side into register 0
    chunk->Append(info.right->Build(visitor, mod));
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    bool left_side_effects = info.left->MayHaveSideEffects();

    // if left is a function call, we have to move rhs to the stack!
    // otherwise, the function call will overwrite what's in register 0.
    Int stack_size_before = 0;
    if (left_side_effects) {
        { // store value of the right hand side on the stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<UInt8>(rp);
            chunk->Append(std::move(instr_push));
        }

        stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    } else {
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
    }

    // load left-hand side into register 1
    chunk->Append(info.left->Build(visitor, mod));

    if (left_side_effects) {
        // now, we increase register usage to load rhs from the stack into register 1.
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        // get register position
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        // load from stack
        Int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        Int diff = stack_size_after - stack_size_before;
        AssertThrow(diff == 1);

        {
            auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
            instr_load_offset->opcode = LOAD_OFFSET;
            instr_load_offset->Accept<UInt8>(rp);
            instr_load_offset->Accept<UInt16>(diff);
            chunk->Append(std::move(instr_load_offset));
        }

        {
            auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
            instr_pop->opcode = POP;
            chunk->Append(std::move(instr_pop));
        }

        // decrement stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

std::unique_ptr<Buildable> Compiler::LoadLeftAndStore(
    AstVisitor *visitor,
    Module *mod,
    Compiler::ExprInfo info
)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    UInt8 rp;

    // load left-hand side into register 0
    chunk->Append(info.left->Build(visitor, mod));
    // get register position
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    { // store value of lhs on the stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(rp);
        chunk->Append(std::move(instr_push));
    }

    Int stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
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
    Int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    Int diff = stack_size_after - stack_size_before;

    AssertThrow(diff == 1);

    {
        auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
        instr_load_offset->opcode = LOAD_OFFSET;
        instr_load_offset->Accept<UInt8>(rp);
        instr_load_offset->Accept<UInt16>(diff);
        chunk->Append(std::move(instr_load_offset));
    }

    { // pop from stack
        auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
        instr_pop->opcode = POP;
        chunk->Append(std::move(instr_pop));
    }

    // decrement stack size
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    return chunk;
}

std::unique_ptr<Buildable> Compiler::BuildBinOp(
    UInt8 opcode,
    AstVisitor *visitor,
    Module *mod,
    Compiler::ExprInfo info
)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AstBinaryExpression *left_as_binop = dynamic_cast<AstBinaryExpression*>(info.left);
    AstBinaryExpression *right_as_binop = dynamic_cast<AstBinaryExpression*>(info.right);

    UInt8 rp;

    if (left_as_binop == nullptr && right_as_binop != nullptr) {
        // if the right hand side is a binary operation,
        // we should build in the rhs first in order to
        // transverse the parse tree.
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
        raw_operation->opcode = opcode;
        raw_operation->Accept<UInt8>(rp); // lhs
        raw_operation->Accept<UInt8>(rp - 1); // rhs
        raw_operation->Accept<UInt8>(rp - 1); // dst

        chunk->Append(std::move(raw_operation));
    } else if (info.right != nullptr && info.right->MayHaveSideEffects()) {
        // lhs must be temporary stored on the stack,
        // to avoid the rhs overwriting it.
        if (info.left->MayHaveSideEffects()) {
            chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<UInt8>(rp - 1); // lhs
            raw_operation->Accept<UInt8>(rp); // rhs
            raw_operation->Accept<UInt8>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        } else {
            // left  doesn't have side effects,
            // so just evaluate right without storing the lhs.
            chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
            
            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<UInt8>(rp); // lhs
            raw_operation->Accept<UInt8>(rp - 1); // rhs
            raw_operation->Accept<UInt8>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        }
    } else {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));

        if (info.right != nullptr) {
            // perform operation
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<UInt8>(rp - 1); // lhs
            raw_operation->Accept<UInt8>(rp); // rhs
            raw_operation->Accept<UInt8>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        }
    }

    return chunk;
}

std::unique_ptr<Buildable> Compiler::PopStack(AstVisitor *visitor, Int amt)
{
    if (amt == 1) {
        auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
        instr_pop->opcode = POP;

        return instr_pop;
    }

    for (Int i = 0; i < amt;) {
        Int j = 0;

        while (j < MathUtil::MaxSafeValue<UInt16>() && i < amt) {
            j++, i++;
        }
        
        if (j > 0) {
            AssertThrow(j <= MathUtil::MaxSafeValue<UInt16>());

            auto instr_sub_sp = BytecodeUtil::Make<RawOperation<>>();
            instr_sub_sp->opcode = SUB_SP;
            instr_sub_sp->Accept<UInt16>(j);

            return instr_sub_sp;
        }
    }

    return nullptr;
}

Compiler::Compiler(AstIterator *ast_iterator, CompilationUnit *compilation_unit)
    : AstVisitor(ast_iterator, compilation_unit)
{
}

Compiler::Compiler(const Compiler &other)
    : AstVisitor(other.m_ast_iterator, other.m_compilation_unit)
{
}

std::unique_ptr<BytecodeChunk> Compiler::Compile()
{
    std::unique_ptr<BytecodeChunk> chunk(new BytecodeChunk);

    Module *mod = m_compilation_unit->GetCurrentModule();
    AssertThrow(mod != nullptr);

    // For all registered types, we need to build their type objects.

    // for (const SymbolTypePtr_t &symbol_type : m_compilation_unit->GetRegisteredTypes()) {
    //     AssertThrow(symbol_type != nullptr);
    //     AssertThrowMsg(symbol_type->GetId() != -1, "Type %s has not been registered", symbol_type->GetName().Data());

    //     // if (symbol_type->IsGenericType()) {
    //     //     continue; // generic types are not buildable
    //     // }

    //     const RC<AstTypeObject> type_object = symbol_type->GetTypeObject().Lock();
    //     AssertThrowMsg(type_object != nullptr, "No AstTypeObject bound to SymbolType %s", symbol_type->GetName().Data());
    //     AssertThrowMsg(type_object->IsVisited(), "AstTypeObject %s has not been visited", type_object->GetName().Data());

    //     chunk->Append(type_object->Build(this, mod));
    // }
    
    while (m_ast_iterator->HasNext()) {
        auto next = m_ast_iterator->Next();
        AssertThrow(next != nullptr);

        chunk->Append(next->Build(this, mod));
    }

    return chunk;
}

} // namespace hyperion::compiler
