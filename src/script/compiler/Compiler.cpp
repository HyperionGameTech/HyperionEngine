#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
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
    const std::vector<std::shared_ptr<AstArgument>> &args)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8_t rp;

    // push a copy of each argument to the stack
    for (size_t i = 0; i < args.size(); i++) {
        auto &arg = args[i];
        AssertThrow(args[i] != nullptr);

        // build in current module (not mod)
        chunk->Append(arg->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // now that it's loaded into the register, make a copy
        // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    return chunk;
}

std::unique_ptr<Buildable> Compiler::BuildArgumentsEnd(
    AstVisitor *visitor,
    Module *mod,
    size_t nargs)
{
     // the reason we decrement the compiler's record of the stack size directly after
    // is because the function body will actually handle the management of the stack size,
    // so that the parameters are actually local variables to the function body.
    for (int i = 0; i < nargs; i++) {
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // pop arguments from stack
    return Compiler::PopStack(visitor, nargs);
}

std::unique_ptr<Buildable> Compiler::BuildCall(
    AstVisitor *visitor,
    Module *mod,
    const std::shared_ptr<AstExpression> &target,
    uint8_t nargs
)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // if no target provided, do not build it in
    if (target != nullptr) {
        chunk->Append(target->Build(visitor, mod));
    }

    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    auto instr_call = BytecodeUtil::Make<RawOperation<>>();
    instr_call->opcode = CALL;
    instr_call->Accept<uint8_t>(rp);
    instr_call->Accept<uint8_t>(nargs);
    chunk->Append(std::move(instr_call));

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::BuildMethodCall(
    AstVisitor *visitor,
    Module *mod,
    const std::shared_ptr<AstMember> &target,
    const std::vector<std::shared_ptr<AstArgument>> &args)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const uint8_t nargs = (uint8_t)args.size();

    uint8_t rp;
    uint8_t self_register;

    AssertThrow(target->GetTarget() != nullptr);

    // build target and stash it
    chunk->Append(target->GetTarget()->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));
    // reserve register
    self_register = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    // push a copy of each argument to the stack
    for (size_t i = 0; i < args.size(); i++) {
        auto &arg = args[i];
        AssertThrow(args[i] != nullptr);

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        
        if (i == 0) {
            // self param
            chunk->Append(target->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));
            // reserve register
            self_register = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        } else {
            // build in current module (not mod)
            chunk->Append(arg->Build(visitor, visitor->GetCompilationUnit()->GetCurrentModule()));
        }

        // now that it's loaded into the register, make a copy
        // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));

        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    }

    // un-reserve self
    rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    // load self (from reserved register)
    auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
    instr_mov_reg->opcode = MOV_REG;
    instr_mov_reg->Accept<uint8_t>(rp); // dst
    instr_mov_reg->Accept<uint8_t>(self_register); // src
    chunk->Append(std::move(instr_mov_reg));
    
    auto instr_call = BytecodeUtil::Make<RawOperation<>>();
    instr_call->opcode = CALL;
    instr_call->Accept<uint8_t>(rp);
    instr_call->Accept<uint8_t>(nargs);
    chunk->Append(std::move(instr_call));

    // the reason we decrement the compiler's record of the stack size directly after
    // is because the function body will actually handle the management of the stack size,
    // so that the parameters are actually local variables to the function body.
    for (int i = 0; i < nargs; i++) {
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // pop arguments from stack
    chunk->Append(Compiler::PopStack(visitor, nargs));

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::LoadMemberFromHash(AstVisitor *visitor, Module *mod, uint32_t hash)
{
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_load_mem_hash = BytecodeUtil::Make<StorageOperation>();
    instr_load_mem_hash->GetBuilder().Load(rp).Member(rp).ByHash(hash);
    return std::move(instr_load_mem_hash);
}

std::unique_ptr<Buildable> Compiler::StoreMemberFromHash(AstVisitor *visitor, Module *mod, uint32_t hash)
{
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_mov_mem_hash = BytecodeUtil::Make<StorageOperation>();
    instr_mov_mem_hash->GetBuilder().Store(rp - 1).Member(rp).ByHash(hash);
    return std::move(instr_mov_mem_hash);
}

std::unique_ptr<Buildable> Compiler::LoadMemberAtIndex(AstVisitor *visitor, Module *mod, int dm_index)
{
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_load_mem = BytecodeUtil::Make<RawOperation<>>();
    instr_load_mem->opcode = LOAD_MEM;
    instr_load_mem->Accept<uint8_t>(rp); // dst
    instr_load_mem->Accept<uint8_t>(rp); // src
    instr_load_mem->Accept<uint8_t>(dm_index); // index
    
    return std::move(instr_load_mem);
}

std::unique_ptr<Buildable> Compiler::StoreMemberAtIndex(AstVisitor *visitor, Module *mod, int dm_index)
{
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    auto instr_mov_mem = BytecodeUtil::Make<RawOperation<>>();
    instr_mov_mem->opcode = MOV_MEM;
    instr_mov_mem->Accept<uint8_t>(rp); // dst
    instr_mov_mem->Accept<uint8_t>(dm_index); // index
    instr_mov_mem->Accept<uint8_t>(rp - 1); // src
    
    return std::move(instr_mov_mem);
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

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8_t rp;

    LabelId end_label = chunk->NewLabel();
    LabelId else_label = chunk->NewLabel();

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
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JNE, end_label));

        // set the label's position to where the else-block would be
        chunk->Append(BytecodeUtil::Make<LabelMarker>(else_label));
        // build the else-block
        chunk->Append(else_part->Build(visitor, mod));
    }

    // set the label's position to after the block,
    // so we can skip it if the condition is false
    chunk->Append(BytecodeUtil::Make<LabelMarker>(end_label));

    return std::move(chunk);
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

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::LoadRightThenLeft(AstVisitor *visitor, Module *mod, Compiler::ExprInfo info)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8_t rp;

    // load right-hand side into register 0
    chunk->Append(info.right->Build(visitor, mod));
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    bool left_side_effects = info.left->MayHaveSideEffects();

    // if left is a function call, we have to move rhs to the stack!
    // otherwise, the function call will overwrite what's in register 0.
    int stack_size_before = 0;
    if (left_side_effects) {
        { // store value of the right hand side on the stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<uint8_t>(rp);
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
        int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int diff = stack_size_after - stack_size_before;
        AssertThrow(diff == 1);

        {
            auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
            instr_load_offset->opcode = LOAD_OFFSET;
            instr_load_offset->Accept<uint8_t>(rp);
            instr_load_offset->Accept<uint16_t>(diff);
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

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::LoadLeftAndStore(
    AstVisitor *visitor,
    Module *mod,
    Compiler::ExprInfo info)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8_t rp;

    // load left-hand side into register 0
    chunk->Append(info.left->Build(visitor, mod));
    // get register position
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    { // store value of lhs on the stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));
    }

    int stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
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
    int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    int diff = stack_size_after - stack_size_before;

    AssertThrow(diff == 1);

    {
        auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
        instr_load_offset->opcode = LOAD_OFFSET;
        instr_load_offset->Accept<uint8_t>(rp);
        instr_load_offset->Accept<uint16_t>(diff);
        chunk->Append(std::move(instr_load_offset));
    }

    { // pop from stack
        auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
        instr_pop->opcode = POP;
        chunk->Append(std::move(instr_pop));
    }

    // decrement stack size
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::BuildBinOp(uint8_t opcode,
    AstVisitor *visitor,
    Module *mod,
    Compiler::ExprInfo info)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AstBinaryExpression *left_as_binop = dynamic_cast<AstBinaryExpression*>(info.left);
    AstBinaryExpression *right_as_binop = dynamic_cast<AstBinaryExpression*>(info.right);

    uint8_t rp;

    if (left_as_binop == nullptr && right_as_binop != nullptr) {
        // if the right hand side is a binary operation,
        // we should build in the rhs first in order to
        // transverse the parse tree.
        chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
        raw_operation->opcode = opcode;
        raw_operation->Accept<uint8_t>(rp); // lhs
        raw_operation->Accept<uint8_t>(rp - 1); // rhs
        raw_operation->Accept<uint8_t>(rp - 1); // dst

        chunk->Append(std::move(raw_operation));
    } else if (info.right != nullptr && info.right->MayHaveSideEffects()) {
        // lhs must be temporary stored on the stack,
        // to avoid the rhs overwriting it.
        if (info.left->MayHaveSideEffects()) {
            chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<uint8_t>(rp - 1); // lhs
            raw_operation->Accept<uint8_t>(rp); // rhs
            raw_operation->Accept<uint8_t>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        } else {
            // left  doesn't have side effects,
            // so just evaluate right without storing the lhs.
            chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
            
            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<uint8_t>(rp); // lhs
            raw_operation->Accept<uint8_t>(rp - 1); // rhs
            raw_operation->Accept<uint8_t>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        }
    } else {
        chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));

        if (info.right != nullptr) {
            // perform operation
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            auto raw_operation = BytecodeUtil::Make<RawOperation<>>();
            raw_operation->opcode = opcode;
            raw_operation->Accept<uint8_t>(rp - 1); // lhs
            raw_operation->Accept<uint8_t>(rp); // rhs
            raw_operation->Accept<uint8_t>(rp - 1); // dst

            chunk->Append(std::move(raw_operation));
        }
    }

    return std::move(chunk);
}

std::unique_ptr<Buildable> Compiler::PopStack(AstVisitor *visitor, int amt)
{
    if (amt == 1) {
        auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
        instr_pop->opcode = POP;

        return std::move(instr_pop);
    }

    for (int i = 0; i < amt;) {
        int j = 0;

        while (j < std::numeric_limits<uint8_t>::max() && i < amt) {
            j++, i++;
        }
        
        if (j > 0) {
            AssertThrow(j <= std::numeric_limits<uint8_t>::max());

            auto instr_pop_n = BytecodeUtil::Make<RawOperation<>>();
            instr_pop_n->opcode = POP_N;
            instr_pop_n->Accept<uint8_t>(j);

            return std::move(instr_pop_n);
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
    
    while (m_ast_iterator->HasNext()) {
        auto next = m_ast_iterator->Next();
        AssertThrow(next != nullptr);

        chunk->Append(next->Build(this, mod));
    }

    return chunk;
}

} // namespace hyperion::compiler
