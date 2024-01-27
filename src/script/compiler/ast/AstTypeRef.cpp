#include <script/compiler/ast/AstTypeRef.hpp>
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

AstTypeRef::AstTypeRef(
    const SymbolTypePtr_t &symbol_type,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_symbol_type(symbol_type),
    m_is_visited(false)
{
}

void AstTypeRef::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);

    m_is_visited = true;
}

std::unique_ptr<Buildable> AstTypeRef::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_symbol_type != nullptr);

    AssertThrowMsg(
        m_symbol_type->GetId() != -1,
        "SymbolType %s not registered, invalid type ref",
        m_symbol_type->ToString(true).Data()
    );

    AssertThrowMsg(
        m_symbol_type->GetTypeObject() != nullptr,
        "SymbolType %s has no type object set, invalid type ref",
        m_symbol_type->ToString(true).Data()
    );

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // Load it from static storage
    auto instr_load_static = BytecodeUtil::Make<StorageOperation>();
    instr_load_static->GetBuilder().Load(rp).Static().ByIndex(m_symbol_type->GetId());
    chunk->Append(std::move(instr_load_static));

    return chunk;
}

void AstTypeRef::Optimize(AstVisitor *visitor, Module *mod)
{
    // do nothing
}

RC<AstStatement> AstTypeRef::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeRef::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeRef::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstTypeRef::GetExprType() const
{
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypePtr_t AstTypeRef::GetHeldType() const
{
    AssertThrow(m_symbol_type != nullptr);

    return m_symbol_type;
}

} // namespace hyperion::compiler
