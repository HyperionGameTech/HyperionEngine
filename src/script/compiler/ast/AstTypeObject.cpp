#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstNil.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeObject::AstTypeObject(
    const SymbolTypePtr_t &symbol_type,
    const std::shared_ptr<AstVariable> &proto,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
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

    chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbol_type->GetName()));

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

    // push the class to the stack
    const auto class_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // use above as self arg so PUSH
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add instruction to store on stack
        auto instr_push = BytecodeUtil::Make<RawOperation<>>();
        instr_push->opcode = PUSH;
        instr_push->Accept<uint8_t>(rp);
        chunk->Append(std::move(instr_push));
    }

    // increment stack size for class
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();


    if (!m_symbol_type->GetMembers().empty()) {
        bool any_members_side_effects = false;

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

            // increment to not overwrite object in register with out class
            rp = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

            const int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
 
            { // load class from stack into register
                auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
                instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(stack_size - class_stack_location);
                chunk->Append(std::move(instr_load_offset));
            }

            { // store data member
                auto instr_mov_mem = BytecodeUtil::Make<RawOperation<>>();
                instr_mov_mem->opcode = MOV_MEM;
                instr_mov_mem->Accept<uint8_t>(rp);
                instr_mov_mem->Accept<uint8_t>(i); // index
                instr_mov_mem->Accept<uint8_t>(obj_reg);
                chunk->Append(std::move(instr_mov_mem));
            }

            // no longer using obj in reg
            rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

            { // comment for debug
                chunk->Append(BytecodeUtil::Make<Comment>("Store member " + std::get<0>(mem)));
            }

            i++;
        }

        // decrease register usage for type
        //visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
    }

    // "return" our class to the last used register before popping
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    const int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    { // load class from stack into register
        auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
        instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(stack_size - class_stack_location);
        chunk->Append(std::move(instr_load_offset));
    }

    // pop class off stack
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    chunk->Append(Compiler::PopStack(visitor, 1));

    chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbol_type->GetName()));

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
