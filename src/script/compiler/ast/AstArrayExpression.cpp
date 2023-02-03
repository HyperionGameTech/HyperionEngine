#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>

#include <unordered_set>

namespace hyperion::compiler {

AstArrayExpression::AstArrayExpression(
    const Array<RC<AstExpression>> &members,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_members(members),
    m_held_type(BuiltinTypes::ANY)
{
}

void AstArrayExpression::Visit(AstVisitor *visitor, Module *mod)
{
    std::unordered_set<SymbolTypePtr_t> held_types;

    for (auto &member : m_members) {
        AssertThrow(member != nullptr);
        member->Visit(visitor, mod);

        if (member->GetExprType() != nullptr) {
            held_types.insert(member->GetExprType());
        } else {
            held_types.insert(BuiltinTypes::ANY);
        }
    }

    for (const auto &it : held_types) {
        AssertThrow(it != nullptr);

        if (m_held_type == BuiltinTypes::UNDEFINED) {
            // `Undefined` invalidates the array type
            break;
        }
        
        if (m_held_type == BuiltinTypes::ANY) {
            // take first item found that is not `Any`
            m_held_type = it;
        } else if (m_held_type->TypeCompatible(*it, false)) {
            m_held_type = SymbolType::TypePromotion(m_held_type, it, true);
        } else {
            // more than one differing type, use Any.
            m_held_type = BuiltinTypes::ANY;
            break;
        }
    }
}

std::unique_ptr<Buildable> AstArrayExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool has_side_effects = MayHaveSideEffects();
    const UInt32 array_size = UInt32(m_members.Size());
    
    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add NEW_ARRAY instruction
        auto instr_new_array = BytecodeUtil::Make<RawOperation<>>();
        instr_new_array->opcode = NEW_ARRAY;
        instr_new_array->Accept<UInt8>(rp);
        instr_new_array->Accept<UInt32>(array_size);
        chunk->Append(std::move(instr_new_array));
    }
    
    int stack_size_before = 0;

    if (has_side_effects) {
        // move to stack temporarily
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
        // claim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    // assign all array items
    int index = 0;
    for (auto &member : m_members) {
        chunk->Append(member->Build(visitor, mod));

        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (has_side_effects) {
            // claim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            int diff = stack_size_after - stack_size_before;
            AssertThrow(diff == 1);

            { // load array from stack back into register
                auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
                instr_load_offset->opcode = LOAD_OFFSET;
                instr_load_offset->Accept<UInt8>(rp);
                instr_load_offset->Accept<UInt16>(diff);
                chunk->Append(std::move(instr_load_offset));
            }

            { // send to the array
                auto instr_mov_array_idx = BytecodeUtil::Make<RawOperation<>>();
                instr_mov_array_idx->opcode = MOV_ARRAYIDX;
                instr_mov_array_idx->Accept<UInt8>(rp);
                instr_mov_array_idx->Accept<UInt32>(index);
                instr_mov_array_idx->Accept<UInt8>(rp - 1);
                chunk->Append(std::move(instr_mov_array_idx));
            }

            // unclaim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        } else {
            // send to the array
            auto instr_mov_array_idx = BytecodeUtil::Make<RawOperation<>>();
            instr_mov_array_idx->opcode = MOV_ARRAYIDX;
            instr_mov_array_idx->Accept<UInt8>(rp - 1);
            instr_mov_array_idx->Accept<UInt32>(index);
            instr_mov_array_idx->Accept<UInt8>(rp);
            chunk->Append(std::move(instr_mov_array_idx));
        }

        index++;
    }

    if (!has_side_effects) {
        // unclaim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    } else {
        // move from stack to register 0
    
        int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int diff = stack_size_after - stack_size_before;
        AssertThrow(diff == 1);
        
        { // load array from stack back into register
            auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
            instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(diff);
            chunk->Append(std::move(instr_load_offset));
        }

        // pop the array from the stack
        chunk->Append(BytecodeUtil::Make<PopLocal>(1));

        // decrement stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return std::move(chunk);
}

void AstArrayExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    for (auto &member : m_members) {
        if (member != nullptr) {
            member->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstArrayExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstArrayExpression::MayHaveSideEffects() const
{
    bool side_effects = false;

    for (const auto &member : m_members) {
        AssertThrow(member != nullptr);
        
        if (member->MayHaveSideEffects()) {
            side_effects = true;
            break;
        }
    }

    return side_effects;
}

SymbolTypePtr_t AstArrayExpression::GetExprType() const
{
    return SymbolType::GenericInstance(
        BuiltinTypes::ARRAY,
        GenericInstanceTypeInfo {
            {
                { "@array_of", m_held_type }
            }
        }
    );
}

} // namespace hyperion::compiler
