#include <script/compiler/ast/AstObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstObject::AstObject(
    const SymbolTypeWeakPtr_t &symbol_type,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_symbol_type(symbol_type)
{
}

void AstObject::Visit(AstVisitor *visitor, Module *mod)
{
    auto sp = m_symbol_type.lock();
    AssertThrow(sp != nullptr);
}

std::unique_ptr<Buildable> AstObject::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    auto sp = m_symbol_type.lock();
    AssertThrow(sp != nullptr);

    int static_id = sp->GetId();
    AssertThrow(static_id != -1);

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    // store object's register location
    UInt8 obj_reg = rp;
    // store the original register location of the object
    const UInt8 original_obj_reg = obj_reg;

    {
        auto instr_type = BytecodeUtil::Make<BuildableType>();
        instr_type->reg = obj_reg;
        instr_type->name = sp->GetName();

        for (const SymbolMember_t &mem : sp->GetMembers()) {
            instr_type->members.PushBack(std::get<0>(mem));
        }

        chunk->Append(std::move(instr_type));
    }

    { // store newly allocated object in same register
        auto instr_new = BytecodeUtil::Make<RawOperation<>>();
        instr_new->opcode = NEW;
        instr_new->Accept<UInt8>(obj_reg); // dst
        instr_new->Accept<UInt8>(obj_reg); // src (holds type)
        chunk->Append(std::move(instr_new));
    }

    { // store the type on the stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<UInt8>(obj_reg); // src
        chunk->Append(std::move(instr_push));
    }

    int obj_stack_loc = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    // increment stack size for the type
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    // for each data member, load the default value
    int i = 0;
    for (const auto &mem : sp->GetMembers()) {
        const SymbolTypePtr_t &mem_type = std::get<1>(mem);
        AssertThrow(mem_type != nullptr);

        // if there has not been an assignment provided,
        // use the default value of the members's type.
        if (std::get<2>(mem) != nullptr) {
            chunk->Append(std::get<2>(mem)->Build(visitor, mod));
        } else {
            // load the data member's default value.
            AssertThrowMsg(mem_type->GetDefaultValue() != nullptr,
                "Default value should not be null (and no assignment was provided)");
            
            chunk->Append(mem_type->GetDefaultValue()->Build(visitor, mod));
        }

        // claim register for the data member
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        // get register position
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        // set obj_reg for future data members
        obj_reg = rp;

        int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int diff = stack_size - obj_stack_loc;
        AssertThrow(diff == 1);

        { // load type from stack
            auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
            instr_load_offset->opcode = LOAD_OFFSET;
            instr_load_offset->Accept<UInt8>(obj_reg);
            instr_load_offset->Accept<UInt16>(diff);
            chunk->Append(std::move(instr_load_offset));
        }

        { // store data member
            auto instr_mov_mem = BytecodeUtil::Make<RawOperation<>>();
            instr_mov_mem->opcode = MOV_MEM;
            instr_mov_mem->Accept<UInt8>(obj_reg);
            instr_mov_mem->Accept<UInt8>(i);
            instr_mov_mem->Accept<UInt8>(rp - 1);
            chunk->Append(std::move(instr_mov_mem));
        }

        { // comment for debug
            chunk->Append(BytecodeUtil::Make<Comment>("Store member " + std::get<0>(mem)));
        }

        // unclaim register
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // get register position
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // swap registers to move the object back to original register
        if (obj_reg != rp) {
            auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
            instr_mov_reg->opcode = MOV_REG;
            instr_mov_reg->Accept<UInt8>(rp);
            instr_mov_reg->Accept<UInt8>(obj_reg);
            chunk->Append(std::move(instr_mov_reg));
            
            obj_reg = rp;
        }

        i++;
    }

    { // pop the type from stack
        auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
        instr_pop->opcode = POP;
        chunk->Append(std::move(instr_pop));
    }

    // decrement stack size for type
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    // check if we have to move the register value back to the original location
    if (obj_reg != original_obj_reg) {
        // move the value in obj_reg into the original location
        auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
        instr_mov_reg->opcode = MOV_REG;
        instr_mov_reg->Accept<UInt8>(original_obj_reg);
        instr_mov_reg->Accept<UInt8>(obj_reg);
        chunk->Append(std::move(instr_mov_reg));
    }

    return chunk;
}

void AstObject::Optimize(AstVisitor *visitor, Module *mod)
{
}

/*void AstObject::SubstituteGenerics(AstVisitor *visitor, Module *mod, 
    const SymbolTypePtr_t &instance)
{
    if (auto sp = m_symbol_type.lock()) {
        if (sp->GetTypeClass() == TYPE_GENERIC_PARAMETER) {
            // substitute generic parameter
            auto base = instance->GetBaseType();
            AssertThrow(base != nullptr);
            AssertThrow(base->GetTypeClass() == TYPE_GENERIC);
            AssertThrow(base->GetGenericInfo().m_params.Size() == 
                sp->GetGenericInstanceInfo().m_param_types.Size());

            size_t index = 0;
            bool type_found = false;

            for (auto &base_param : base->GetGenericInfo().m_params) {
                AssertThrow(base_param != nullptr);

                if (base_param->GetName() == sp->GetName()) {
                    // substitute in supplied type
                    m_symbol_type = sp->GetGenericInstanceInfo().m_param_types[index];
                    type_found = true;
                    break;
                }

                index++;
            }

            AssertThrow(type_found);
        }
    }
}*/

RC<AstStatement> AstObject::Clone() const
{
    return CloneImpl();
}

Tribool AstObject::IsTrue() const
{
    return Tribool::True();
}

bool AstObject::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstObject::GetExprType() const
{
    auto sp = m_symbol_type.lock();
    AssertThrow(sp != nullptr);

    return sp;
}

} // namespace hyperion::compiler
