#include <Lang/compiler/ast/AstTypeRef.hpp>
#include <Lang/compiler/ast/AstVariableDeclaration.hpp>
#include <Lang/compiler/AstVisitor.hpp>
#include <Lang/compiler/Module.hpp>
#include <Lang/compiler/Configuration.hpp>
#include <Lang/compiler/SemanticAnalyzer.hpp>

#include <Lang/compiler/emit/BytecodeChunk.hpp>
#include <Lang/compiler/emit/BytecodeUtil.hpp>
#include <Lang/compiler/emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <iostream>

namespace Hyperion {

AstTypeRef::AstTypeRef(
    const SymbolType* symbolType,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_symbolType(symbolType)
{
}

void AstTypeRef::Visit(AstVisitor* visitor, Module* mod)
{
    AssertDebug(m_symbolType != nullptr);

    // to catch early, if symbol type is not registered (would be mem leak):
    AssertDebug(m_symbolType->IsRegistered());
}

UniquePtr<Buildable> AstTypeRef::Build(AstVisitor* visitor, Module* mod)
{
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    const SymbolType* unaliased = m_symbolType->GetUnaliased();

    // User-defined types have a ClassRef stored as a local
    if (const RC<AstVariableDeclaration>& classRefDecl = unaliased->GetClassRefDecl())
    {
        const Identifier* identifier = classRefDecl->GetIdentifier();
        Assert(identifier != nullptr);

        const int stackLocation = identifier->GetStackLocation();
        Assert(stackLocation != -1);

        const int offset = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - stackLocation;

        UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

        if (identifier->GetFlags() & IdentifierFlags::DECLARED_IN_FUNCTION)
        {
            auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
            instrLoadOffset->GetBuilder().Load(rp).Local().ByOffset(offset);
            chunk->Append(std::move(instrLoadOffset));
        }
        else
        {
            auto instrLoadIndex = BytecodeUtil::Make<StorageOperation>();
            instrLoadIndex->GetBuilder().Load(rp).Local().ByIndex(stackLocation);
            chunk->Append(std::move(instrLoadIndex));
        }

        return chunk;
    }

    const String& className = BuiltinTypes::GetNativeClassNameForType(unaliased);
    return BytecodeUtil::Make<LoadClass>(rp, StringHash(className));
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

const SymbolType* AstTypeRef::GetExprType() const
{
    return BuiltinTypes::s_classType;
}

const SymbolType* AstTypeRef::GetHeldType() const
{
    return m_symbolType;
}

} // namespace Hyperion
