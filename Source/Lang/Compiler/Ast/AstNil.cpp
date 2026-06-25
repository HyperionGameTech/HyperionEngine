#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstUnsignedInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>

#include <Core/Types.hpp>

#include <AstNil.generated.inl>

namespace Hyperion {

AstNil::AstNil(const SourceLocation& location)
    : AstConstant(ConstantValue(int64(0), CBS_32), location)
{
}

UniquePtr<Buildable> AstNil::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstNull>(rp);
}

RC<AstStatement> AstNil::Clone() const
{
    return CloneImpl();
}

Tribool AstNil::IsTrue() const
{
    return Tribool::False();
}

bool AstNil::IsNumber() const
{
    return false;
}

const SymbolType* AstNil::GetExprType() const
{
    return BuiltinTypes::s_nullType;
}

} // namespace Hyperion
