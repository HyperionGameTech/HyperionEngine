#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

AstFalse::AstFalse(const SourceLocation& location)
    : AstConstant(ConstantValue(false, CBS_8), location)
{
}

UniquePtr<Buildable> AstFalse::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, false);
}

RC<AstStatement> AstFalse::Clone() const
{
    return CloneImpl();
}

Tribool AstFalse::IsTrue() const
{
    return Tribool::False();
}

bool AstFalse::IsNumber() const
{
    return false;
}

const SymbolType* AstFalse::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

} // namespace Hyperion
