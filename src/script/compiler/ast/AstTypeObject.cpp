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
#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeObject::AstTypeObject(
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& baseSymbolType,
    const SourceLocation& location)
    : AstTypeObject(symbolType, baseSymbolType, nullptr, false, location)
{
}

AstTypeObject::AstTypeObject(
    const SymbolTypeRef& symbolType,
    const SymbolTypeRef& baseSymbolType,
    const SymbolTypeRef& enumUnderlyingType,
    bool isProxyClass,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_symbolType(symbolType),
      m_baseSymbolType(baseSymbolType),
      m_enumUnderlyingType(enumUnderlyingType),
      m_isProxyClass(isProxyClass),
      m_isVisited(false)
{
}

void AstTypeObject::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(!m_isVisited);

    Assert(m_symbolType != nullptr);
    Assert(m_symbolType->GetId() == -1, "Type %s already registered", m_symbolType->ToString(true).Data());

    if (m_baseSymbolType != nullptr)
    {
        SymbolTypeRef baseType = m_baseSymbolType->GetUnaliased();
        m_baseTypeRef.Reset(new AstTypeRef(baseType, m_location));
        m_baseTypeRef->Visit(visitor, mod);
    }

    // @TODO Ensure that the base type is able to be used (e.g CLASS_TYPE)

    m_memberExpressions.Resize(m_symbolType->GetMembers().Size());

    for (SizeType index = 0; index < m_symbolType->GetMembers().Size(); index++)
    {
        const SymbolTypeMember& member = m_symbolType->GetMembers()[index];

        SymbolTypeRef memberType = member.type;
        Assert(memberType != nullptr);
        memberType = memberType->GetUnaliased();

        RC<AstExpression> previousExpr;

        if (member.expr != nullptr)
        {
            previousExpr = member.expr;
        }
        else
        {
            previousExpr = memberType->GetDefaultValue();
        }

        Assert(
            previousExpr != nullptr,
            "No assigned value for member %s and no default value for type (%s)",
            member.name.Data(),
            memberType->ToString(true).Data());

        m_memberExpressions[index] = CloneAstNode(previousExpr);
        m_memberExpressions[index]->SetExpressionFlags(previousExpr->GetExpressionFlags());
    }

    for (const RC<AstExpression>& expr : m_memberExpressions)
    {
        Assert(expr != nullptr);

        expr->Visit(visitor, mod);
    }

    // register the type
    visitor->GetCompilationUnit()->RegisterType(m_symbolType);
    Assert(m_symbolType->GetId() != -1);

    m_isVisited = true;
}

UniquePtr<Buildable> AstTypeObject::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);
    Assert(m_symbolType->GetId() != -1);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // // get active register
    // uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // // if (m_symbolType->GetId() != -1) {
    // //     chunk->Append(BytecodeUtil::Make<Comment>("Load class " + m_symbolType->GetName() + (m_isProxyClass ? " <Proxy>" : "")));

    // //     // already built, we can just load it from the static table
    // //     auto instrLoadStatic = BytecodeUtil::Make<StorageOperation>();
    // //     instrLoadStatic->GetBuilder().Load(rp).Static().ByIndex(m_symbolType->GetId());
    // //     chunk->Append(std::move(instrLoadStatic));

    // //     return chunk;
    // // }

    // chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbolType->GetName() + (m_isProxyClass ? " <Proxy>" : "")));

    // if (m_baseTypeRef != nullptr)
    // {
    //     chunk->Append(m_baseTypeRef->Build(visitor, mod));
    // }
    // else
    // {
    //     chunk->Append(BytecodeUtil::Make<ConstNull>(rp));
    // }

    // rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // // store object's register location
    // const uint8 objReg = rp;

    // { // load the type into register objReg
    //     auto instrType = BytecodeUtil::Make<BuildableType>();
    //     instrType->reg = objReg;
    //     instrType->name = m_symbolType->GetName();

    //     for (const SymbolTypeMember& mem : m_symbolType->GetMembers())
    //     {
    //         instrType->members.PushBack(mem.name);
    //     }

    //     chunk->Append(std::move(instrType));
    // }

    // Assert(m_memberExpressions.Size() == m_symbolType->GetMembers().Size());

    // if (m_memberExpressions.Any())
    // {
    //     // push the class to the stack
    //     const int classStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    //     // use above as self arg so PUSH
    //     rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     { // add instruction to store on stack
    //         auto instrPush = BytecodeUtil::Make<RawOperation<>>();
    //         instrPush->opcode = PUSH;
    //         instrPush->Accept<uint8>(rp);
    //         chunk->Append(std::move(instrPush));
    //     }

    //     // increment stack size for class
    //     visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    //     for (SizeType index = 0; index < m_memberExpressions.Size(); index++)
    //     {
    //         Assert(index < MathUtil::MaxSafeValue<uint8>(), "Argument out of bouds of max arguments");

    //         const RC<AstExpression>& mem = m_memberExpressions[index];
    //         Assert(mem != nullptr);

    //         chunk->Append(mem->Build(visitor, mod));

    //         // increment to not overwrite object in register with out class
    //         rp = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

    //         const int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    //         { // load class from stack into register
    //             auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
    //             instrLoadOffset->GetBuilder().Load(rp).Local().ByOffset(stackSize - classStackLocation);
    //             chunk->Append(std::move(instrLoadOffset));
    //         }

    //         { // store data member
    //             auto instrMovMem = BytecodeUtil::Make<RawOperation<>>();
    //             instrMovMem->opcode = MOV_MEM;
    //             instrMovMem->Accept<uint8>(rp);
    //             instrMovMem->Accept<uint8>(uint8(index));
    //             instrMovMem->Accept<uint8>(objReg);
    //             chunk->Append(std::move(instrMovMem));
    //         }

    //         // no longer using obj in reg
    //         rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

    //         { // comment for debug
    //             chunk->Append(BytecodeUtil::Make<Comment>("Store member " + m_symbolType->GetMembers()[index].name));
    //         }
    //     }

    //     // "return" our class to the last used register before popping
    //     rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     const int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    //     { // load class from stack into register so it is back in rp
    //         auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
    //         instrLoadOffset->GetBuilder()
    //             .Load(rp)
    //             .Local()
    //             .ByOffset(stackSize - classStackLocation);

    //         chunk->Append(std::move(instrLoadOffset));
    //     }

    //     // pop class off stack
    //     visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    //     chunk->Append(Compiler::PopStack(visitor, 1));
    // }
    // else
    // {
    //     Assert(objReg == visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister());
    // }

    // { // store class in static table
    //     chunk->Append(BytecodeUtil::Make<Comment>("Store class " + m_symbolType->GetName() + " in static data at index " + String::ToString(m_symbolType->GetId())));

    //     auto instrStoreStatic = BytecodeUtil::Make<StorageOperation>();
    //     instrStoreStatic->GetBuilder().Store(rp).Static().ByIndex(m_symbolType->GetId());
    //     chunk->Append(std::move(instrStoreStatic));
    // }

    // chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbolType->GetName()));

    return chunk;
}

void AstTypeObject::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_baseTypeRef != nullptr)
    {
        m_baseTypeRef->Optimize(visitor, mod);
    }

    for (const RC<AstExpression>& expr : m_memberExpressions)
    {
        if (expr == nullptr)
        {
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

SymbolTypeRef AstTypeObject::GetExprType() const
{
    if (m_baseSymbolType == nullptr)
    {
        return BuiltinTypes::CLASS_TYPE;
    }

    return m_baseSymbolType;
}

SymbolTypeRef AstTypeObject::GetHeldType() const
{
    Assert(m_symbolType != nullptr);

    return m_symbolType;
}

} // namespace hyperion::compiler
