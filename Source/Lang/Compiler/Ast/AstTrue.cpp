#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <AstTrue.generated.inl>

namespace Hyperion {

AstTrue::AstTrue(const SourceLocation& location)
    : AstConstant(ConstantValue(true, CBS_8), location)
{
}

UniquePtr<Buildable> AstTrue::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, true);
}

RC<AstStatement> AstTrue::Clone() const
{
    return CloneImpl();
}

Tribool AstTrue::IsTrue() const
{
    return Tribool::True();
}

bool AstTrue::IsNumber() const
{
    return false;
}

const SymbolType* AstTrue::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

} // namespace Hyperion
