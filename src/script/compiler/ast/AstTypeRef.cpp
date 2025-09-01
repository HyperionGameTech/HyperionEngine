#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

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
    // do nothing, type refs do not produce any bytecode

    return nullptr;
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
