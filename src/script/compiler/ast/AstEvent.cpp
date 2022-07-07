#include <script/compiler/ast/AstEvent.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstConstantEvent::AstConstantEvent(const std::shared_ptr<AstConstant> &key,
    const std::shared_ptr<AstFunctionExpression> &trigger,
    const SourceLocation &location)
    : AstEvent(trigger, location),
      m_key(key)
{
}

std::shared_ptr<AstExpression> AstConstantEvent::GetKey() const
{
    return m_key;
}

std::string AstConstantEvent::GetKeyName() const
{
    AssertThrow(m_key != nullptr);

    if (AstString *as_string = dynamic_cast<AstString*>(m_key.get())) {
        return as_string->GetValue();
    } else if (AstFloat *as_float = dynamic_cast<AstFloat*>(m_key.get())) {
        return std::to_string(as_float->FloatValue());
    } else {
        return std::to_string(m_key->IntValue());
    }
}

void AstConstantEvent::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_key != nullptr);
    m_key->Visit(visitor, mod);

    AstEvent::Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstConstantEvent::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    return AstEvent::Build(visitor, mod);
}

void AstConstantEvent::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_key != nullptr);
    m_key->Optimize(visitor, mod);

    AstEvent::Optimize(visitor, mod);
}

Pointer<AstStatement> AstConstantEvent::Clone() const
{
    return CloneImpl();
}


AstEvent::AstEvent(const std::shared_ptr<AstFunctionExpression> &trigger,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_trigger(trigger)
{
}

void AstEvent::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_trigger != nullptr);
    m_trigger->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstEvent::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_trigger != nullptr);
    return m_trigger->Build(visitor, mod);
}

void AstEvent::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_trigger != nullptr);
    m_trigger->Optimize(visitor, mod);
}

Tribool AstEvent::IsTrue() const
{
    return Tribool::True();
}

bool AstEvent::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstEvent::GetExprType() const
{
    return BuiltinTypes::EVENT;
}

} // namespace hyperion::compiler
