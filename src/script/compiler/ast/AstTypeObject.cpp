#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

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
    const SymbolTypePtr_t &base_symbol_type,
    const SourceLocation &location
) : AstTypeObject(symbol_type, base_symbol_type, nullptr, false, location)
{
}

AstTypeObject::AstTypeObject(
    const SymbolTypePtr_t &symbol_type,
    const SymbolTypePtr_t &base_symbol_type,
    const SymbolTypePtr_t &enum_underlying_type,
    bool is_proxy_class,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_symbol_type(symbol_type),
    m_base_symbol_type(base_symbol_type),
    m_enum_underlying_type(enum_underlying_type),
    m_is_proxy_class(is_proxy_class),
    m_is_visited(false)
{
}

void AstTypeObject::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(!m_is_visited);

    AssertThrow(m_symbol_type != nullptr);
    AssertThrowMsg(m_symbol_type->GetId() == -1, "Type %s already registered", m_symbol_type->ToString(true).Data());

    if (m_base_symbol_type != nullptr) {
        SymbolTypePtr_t base_type = m_base_symbol_type->GetUnaliased();
        m_base_type_ref.Reset(new AstTypeRef(base_type, m_location));
        m_base_type_ref->Visit(visitor, mod);
    }

    // @TODO Ensure that the base type is able to be used (e.g CLASS_TYPE)

    m_member_expressions.Resize(m_symbol_type->GetMembers().Size());

    for (SizeType index = 0; index < m_symbol_type->GetMembers().Size(); index++) {
        const SymbolTypeMember &member = m_symbol_type->GetMembers()[index];

        SymbolTypePtr_t member_type = member.type;
        AssertThrow(member_type != nullptr);
        member_type = member_type->GetUnaliased();

        RC<AstExpression> previous_expr;

        if (member.expr != nullptr) {
            previous_expr = member.expr;
        } else {
            previous_expr = member_type->GetDefaultValue();
        }

        AssertThrowMsg(
            previous_expr != nullptr,
            "No assigned value for member %s and no default value for type (%s)",
            member.name.Data(),
            member_type->ToString(true).Data()
        );

        m_member_expressions[index] = CloneAstNode(previous_expr);
        m_member_expressions[index]->SetExpressionFlags(previous_expr->GetExpressionFlags());
    }

    for (const RC<AstExpression> &expr : m_member_expressions) {
        AssertThrow(expr != nullptr);

        expr->Visit(visitor, mod);
    }

    // register the type
    visitor->GetCompilationUnit()->RegisterType(m_symbol_type);
    AssertThrow(m_symbol_type->GetId() != -1);

    m_is_visited = true;
}

std::unique_ptr<Buildable> AstTypeObject::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);
    AssertThrow(m_symbol_type->GetId() != -1);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    
    // if (m_symbol_type->GetId() != -1) {
    //     chunk->Append(BytecodeUtil::Make<Comment>("Load class " + m_symbol_type->GetName() + (m_is_proxy_class ? " <Proxy>" : "")));

    //     // already built, we can just load it from the static table
    //     auto instr_load_static = BytecodeUtil::Make<StorageOperation>();
    //     instr_load_static->GetBuilder().Load(rp).Static().ByIndex(m_symbol_type->GetId());
    //     chunk->Append(std::move(instr_load_static));

    //     return chunk;
    // }

    chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbol_type->GetName() + (m_is_proxy_class ? " <Proxy>" : "")));

    if (m_base_type_ref != nullptr) {
        chunk->Append(m_base_type_ref->Build(visitor, mod));
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

        for (const SymbolTypeMember &mem : m_symbol_type->GetMembers()) {
            instr_type->members.PushBack(mem.name);
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
                chunk->Append(BytecodeUtil::Make<Comment>("Store member " + m_symbol_type->GetMembers()[index].name));
            }
        }

        // "return" our class to the last used register before popping
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const int stack_size = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        { // load class from stack into register so it is back in rp
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

    { // store class in static table
        chunk->Append(BytecodeUtil::Make<Comment>("Store class " + m_symbol_type->GetName() + " in static data at index " + String::ToString(m_symbol_type->GetId())));

        auto instr_store_static = BytecodeUtil::Make<StorageOperation>();
        instr_store_static->GetBuilder().Store(rp).Static().ByIndex(m_symbol_type->GetId());
        chunk->Append(std::move(instr_store_static));
    }

    chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbol_type->GetName()));

    return chunk;
}

void AstTypeObject::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_base_type_ref != nullptr) {
        m_base_type_ref->Optimize(visitor, mod);
    }

    for (const RC<AstExpression> &expr : m_member_expressions) {
        if (expr == nullptr) {
            continue;
        }

        expr->Optimize(visitor, mod);
    }
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
    if (m_base_symbol_type == nullptr) {
        return BuiltinTypes::CLASS_TYPE;
    }

    return m_base_symbol_type;
}

SymbolTypePtr_t AstTypeObject::GetHeldType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

} // namespace hyperion::compiler
