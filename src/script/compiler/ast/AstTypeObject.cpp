#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstNil.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeObject::AstTypeObject(const SymbolTypePtr_t &symbol_type,
    const std::shared_ptr<AstVariable> &proto,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_symbol_type(symbol_type),
      m_proto(proto)
{
}

void AstTypeObject::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);

    if (m_proto != nullptr) {
        m_proto->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstTypeObject::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_symbol_type != nullptr);

    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //const uint8_t proto_rp = rp;

    // load the type object using new(proto) instruction, where proto is the base class of this.
    // TODO: base class stuff. for now load null as proto of this type
    /*if (m_proto != nullptr) {
        chunk->Append(m_proto->Build(visitor, mod));
    } else {
        // load null
        chunk->Append(AstNil(m_location).Build(visitor, mod));
    }

    // increase register usage for the prototype
    rp = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    // TODO: instantiate static members. this class could prob be merged w/ AstObject

    auto instr_new = BytecodeUtil::Make<RawOperation<>>();
    instr_new->opcode = NEW;
    instr_new->Accept<uint8_t>(rp); // dst
    instr_new->Accept<uint8_t>(proto_rp); // src (holds proto)
    chunk->Append(std::move(instr_new));*/

    /*auto instr_type = BytecodeUtil::Make<BuildableType>();
    instr_type->reg = rp;
    instr_type->name = sp->GetName();
    for (const SymbolMember_t &mem : sp->GetMembers()) {
        instr_type->members.push_back(std::get<0>(mem));
    }

    chunk->Append(std::move(instr_type));*/

    //visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();


    // get active register
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    // store object's register location
    const uint8_t obj_reg = rp;
    // store the original register location of the object
    const uint8_t original_obj_reg = obj_reg;

    {
        auto instr_type = BytecodeUtil::Make<BuildableType>();
        instr_type->reg = obj_reg;
        instr_type->name = m_symbol_type->GetName();

        for (const SymbolMember_t &mem : m_symbol_type->GetMembers()) {
            instr_type->members.push_back(std::get<0>(mem));
        }

        chunk->Append(std::move(instr_type));
    }

    if (!m_symbol_type->GetMembers().empty()) {
        bool any_members_side_effects = false;

        /*{ // store the type on the stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<uint8_t>(obj_reg); // src
            chunk->Append(std::move(instr_push));
        }*/

        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        /*int obj_stack_loc = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();*/
        // increment stack size for the type
        /*visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
        */
        // for each data member, load the default value
        int i = 0;
        for (const auto &mem : m_symbol_type->GetMembers()) {
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
            //visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            // get register position
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
            // set obj_reg for future data members
            /*obj_reg = rp;*/

            /*int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            int diff = stack_size - obj_stack_loc;
            AssertThrow(diff == 1);*/

            /*{ // load type from stack
                auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
                instr_load_offset->opcode = LOAD_OFFSET;
                instr_load_offset->Accept<uint8_t>(obj_reg);
                instr_load_offset->Accept<uint16_t>(diff);
                chunk->Append(std::move(instr_load_offset));
            }*/

            { // store data member
                auto instr_mov_mem = BytecodeUtil::Make<RawOperation<>>();
                instr_mov_mem->opcode = MOV_MEM;
                instr_mov_mem->Accept<uint8_t>(obj_reg);
                instr_mov_mem->Accept<uint8_t>(i); // index
                instr_mov_mem->Accept<uint8_t>(rp/* - 1*/);
                chunk->Append(std::move(instr_mov_mem));
            }

            // unclaim register for data member
            //visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            // get register position
            //rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            // swap registers to move the object back to original register
            /*if (obj_reg != rp) {
                auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
                instr_mov_reg->opcode = MOV_REG;
                instr_mov_reg->Accept<uint8_t>(rp);
                instr_mov_reg->Accept<uint8_t>(obj_reg);
                chunk->Append(std::move(instr_mov_reg));
                
                obj_reg = rp;
            }*/

            i++;
        }

        // decrease register usage for type
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

        /*{ // pop the type from stack
            auto instr_pop = BytecodeUtil::Make<RawOperation<>>();
            instr_pop->opcode = POP;
            chunk->Append(std::move(instr_pop));
        }

        // decrement stack size for type
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();*/

        // check if we have to move the register value back to the original location
        /*if (obj_reg != original_obj_reg) {
            // move the value in obj_reg into the original location
            auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
            instr_mov_reg->opcode = MOV_REG;
            instr_mov_reg->Accept<uint8_t>(original_obj_reg);
            instr_mov_reg->Accept<uint8_t>(obj_reg);
            chunk->Append(std::move(instr_mov_reg));
        }*/

    }

    return std::move(chunk);
}

void AstTypeObject::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstTypeObject::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeObject::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeObject::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstTypeObject::GetExprType() const
{
    AssertThrow(m_symbol_type != nullptr);
    return m_symbol_type->GetBaseType();
}

} // namespace hyperion::compiler
