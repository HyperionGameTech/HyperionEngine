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

    m_member_expressions.Resize(m_symbol_type->GetMembers().Size());

    for (SizeType index = 0; index < m_symbol_type->GetMembers().Size(); index++) {
        const SymbolMember_t &member = m_symbol_type->GetMembers()[index];

        const SymbolTypePtr_t &member_type = std::get<1>(member);
        AssertThrow(member_type != nullptr);

        AstExpression *previous_expression = nullptr;

        if (std::get<2>(member) != nullptr) {
            previous_expression = std::get<2>(member).Get();
        } else {
            previous_expression = member_type->GetDefaultValue().Get();
        }

        if (previous_expression != nullptr) {
            m_member_expressions[index] = CloneAstNode(previous_expression);
            m_member_expressions[index]->SetExpressionFlags(previous_expression->GetExpressionFlags());
        }
    }

    for (const RC<AstExpression> &expr : m_member_expressions) {
        if (expr == nullptr) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                m_location
            ));

            continue;
        }

        expr->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstTypeObject::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();
    chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbol_type->GetName() + (m_is_proxy_class ? " <Proxy>" : "")));

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    if (m_proto != nullptr) {
        chunk->Append(m_proto->Build(visitor, mod));
    } else {
        chunk->Append(BytecodeUtil::Make<ConstNull>(rp));
    }

    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // store object's register location
    const UInt8 obj_reg = rp;

    { // load the type into register obj_reg
        auto instr_type = BytecodeUtil::Make<BuildableType>();
        instr_type->reg = obj_reg;
        instr_type->name = m_symbol_type->GetName();

        for (const SymbolMember_t &mem : m_symbol_type->GetMembers()) {
            instr_type->members.PushBack(std::get<0>(mem));
        }

        chunk->Append(std::move(instr_type));
    }

    AssertThrow(m_member_expressions.Size() == m_symbol_type->GetMembers().Size());

    if (m_member_expressions.Any()) {
        // push the class to the stack
        const Int class_stack_location = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

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

        for (SizeType index = 0; index < m_member_expressions.Size(); index++) {
            AssertThrowMsg(index < MathUtil::MaxSafeValue<UInt8>(), "Argument out of bouds of max arguments");

            const RC<AstExpression> &mem = m_member_expressions[index];
            AssertThrow(mem != nullptr);

            chunk->Append(mem->Build(visitor, mod));

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
                instr_mov_mem->Accept<UInt8>(UInt8(index));
                instr_mov_mem->Accept<UInt8>(obj_reg);
                chunk->Append(std::move(instr_mov_mem));
            }

            // no longer using obj in reg
            rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

            { // comment for debug
                chunk->Append(BytecodeUtil::Make<Comment>("Store member " + std::get<0>(m_symbol_type->GetMembers()[index])));
            }
        }

        // "return" our class to the last used register before popping
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        { // load class from stack into register
            auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
            instr_load_offset->GetBuilder()
                .Load(rp)
                .Local()
                .ByOffset(stack_size - class_stack_location);

            chunk->Append(std::move(instr_load_offset));
        }

        // pop class off stack
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        chunk->Append(Compiler::PopStack(visitor, 1));
    } else {
        AssertThrow(obj_reg == visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister());
    }

    chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbol_type->GetName()));

    return chunk;
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
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypePtr_t AstTypeObject::GetHeldType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

const AstTypeObject *AstTypeObject::ExtractTypeObject() const
{
    return this;
}

} // namespace hyperion::compiler
