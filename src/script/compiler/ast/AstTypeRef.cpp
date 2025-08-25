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
#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeRef::AstTypeRef(
    const SymbolTypeRef& symbolType,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_symbolType(symbolType),
      m_isVisited(false)
{
}

void AstTypeRef::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);

    m_isVisited = true;
}

UniquePtr<Buildable> AstTypeRef::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);

//    Assert(
//        m_symbolType->GetId() != -1,
//        "SymbolType %s not registered, invalid type ref",
//        m_symbolType->ToString(true).Data());

    // Assert(
    //     m_symbolType->GetTypeObject() != nullptr,
    //     "SymbolType %s has no type object set, invalid type ref",
    //     m_symbolType->ToString(true).Data());

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

//    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
//
//    // Load it from static storage
//    auto instrLoadStatic = BytecodeUtil::Make<StorageOperation>();
//    instrLoadStatic->GetBuilder().Load(rp).Static().ByIndex(m_symbolType->GetId());
//    chunk->Append(std::move(instrLoadStatic));

    return chunk;
}

void AstTypeRef::Optimize(AstVisitor* visitor, Module* mod)
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

SymbolTypeRef AstTypeRef::GetExprType() const
{
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypeRef AstTypeRef::GetHeldType() const
{
    Assert(m_symbolType != nullptr);

    return m_symbolType;
}

} // namespace hyperion::compiler
