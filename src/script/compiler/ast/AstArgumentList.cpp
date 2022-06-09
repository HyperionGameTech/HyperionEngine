#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion::compiler {

AstArgumentList::AstArgumentList(
    const std::vector<std::shared_ptr<AstArgument>> &args,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_args(args)
{
}

void AstArgumentList::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    for (const std::shared_ptr<AstArgument> &arg : m_args) {
        AssertThrow(arg != nullptr);
        arg->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstArgumentList::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    for (const std::shared_ptr<AstArgument> &arg : m_args) {
        AssertThrow(arg != nullptr);
        chunk->Append(arg->Build(visitor, mod));
    }

    return std::move(chunk);
}

void AstArgumentList::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    for (const std::shared_ptr<AstArgument> &arg : m_args) {
        AssertThrow(arg != nullptr);
        arg->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstArgumentList::Clone() const
{
    return CloneImpl();
}

Tribool AstArgumentList::IsTrue() const
{
    return Tribool::True();
}

bool AstArgumentList::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstArgumentList::GetExprType() const
{
    return BuiltinTypes::ANY;
}

} // namespace hyperion::compiler
