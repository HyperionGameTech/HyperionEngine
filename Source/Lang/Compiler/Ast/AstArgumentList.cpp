#include <Lang/Compiler/Ast/AstArgumentList.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <AstArgumentList.generated.inl>

namespace Hyperion {

AstArgumentList::AstArgumentList(
    const Array<Handle<AstArgument>>& args,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_args(args)
{
}

void AstArgumentList::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    for (const Handle<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        arg->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstArgumentList::Build(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    for (const Handle<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        chunk->Append(arg->Build(visitor, mod));
    }

    return chunk;
}

void AstArgumentList::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    for (const Handle<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        arg->Optimize(visitor, mod);
    }
}

Handle<AstStatement> AstArgumentList::Clone() const
{
    return CloneImpl();
}

Tribool AstArgumentList::IsTrue() const
{
    return Tribool::True();
}

bool AstArgumentList::MayHaveSideEffects() const
{
    for (const Handle<AstArgument>& arg : m_args)
    {
        Assert(arg != nullptr);

        if (arg->IsPlaceholderArgument())
        {
            continue;
        }

        if (arg->MayHaveSideEffects())
        {
            return true;
        }
    }

    return false;
}

const SymbolType* AstArgumentList::GetExprType() const
{
    return BuiltinTypes::s_anyType;
}

} // namespace Hyperion
