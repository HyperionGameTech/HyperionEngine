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
#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeObject::AstTypeObject(
    const SymbolTypePtr_t &symbol_type,
    const RC<AstVariable> &proto,
    const SourceLocation &location
) : AstTypeObject(symbol_type, proto, nullptr, false, location)
{
}

AstTypeObject::AstTypeObject(
    const SymbolTypePtr_t &symbol_type,
    const RC<AstVariable> &proto,
    const SymbolTypePtr_t &enum_underlying_type,
    bool is_proxy_class,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_symbol_type(symbol_type),
    m_proto(proto),
    m_enum_underlying_type(enum_underlying_type),
    m_is_proxy_class(is_proxy_class)
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
    AssertThrow(m_symbol_type != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();
    chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbol_type->GetName() + (m_is_proxy_class ? " <Proxy>" : "")));

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    // store object's register location
    const UInt8 obj_reg = rp;

    { // load the type into register obj_reg
        auto instr_type = BytecodeUtil::Make<BuildableType>();
        instr_type->reg = obj_reg;
        instr_type->name = m_symbol_type->GetName();

        for (const SymbolMember_t &mem : m_symbol_type->GetMembers()) {
            instr_type->members.push_back(std::get<0>(mem));
        }

        chunk->Append(std::move(instr_type));
    }

    if (!m_symbol_type->GetMembers().empty()) {
        // push the class to the stack
        const auto class_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        // use above as self arg so PUSH
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // add instruction to store on stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<UInt8>(rp);
            chunk->Append(std::move(instr_push));
        }

        // increment stack size for class
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

        int member_index = 0;

        for (const auto &mem : m_symbol_type->GetMembers()) {
            // TODO: Do some for all base classes' members.

            const SymbolTypePtr_t &mem_type = std::get<1>(mem);
            AssertThrow(mem_type != nullptr);

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
                instr_mov_mem->Accept<UInt8>(rp);
                instr_mov_mem->Accept<UInt8>(member_index);
                instr_mov_mem->Accept<UInt8>(obj_reg);
                chunk->Append(std::move(instr_mov_mem));
            }

            // no longer using obj in reg
            rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

            { // comment for debug
                chunk->Append(BytecodeUtil::Make<Comment>("Store member " + std::get<0>(mem)));
            }

            member_index++;
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
    } else {
        AssertThrow(obj_reg == visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister());
    }

    chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbol_type->GetName()));

    return std::move(chunk);
}

void AstTypeObject::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstTypeObject::Clone() const
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
