#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>
#include <util/utf8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstPrototypeSpecification::AstPrototypeSpecification(
    const std::shared_ptr<AstExpression> &proto,
    const SourceLocation &location)
    : AstStatement(location),
      m_proto(proto)
{
}

void AstPrototypeSpecification::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_proto != nullptr);
    m_proto->Visit(visitor, mod);

    SymbolTypePtr_t constructor_type;

    if (const AstTypeObject *as_type_object = dynamic_cast<const AstTypeObject*>(m_proto->GetValueOf())) {
        constructor_type = as_type_object->GetHeldType();
    } else {
        AssertThrow(m_proto->GetExprType() != nullptr);
        constructor_type = m_proto->GetExprType();
    }

    //printf("expr type: %s\n", m_proto->GetExprType()->GetName().c_str());

    AssertThrow(constructor_type != nullptr);

    m_symbol_type = BuiltinTypes::UNDEFINED;
    m_prototype_type = BuiltinTypes::UNDEFINED;

    if (constructor_type != BuiltinTypes::ANY_TYPE && !constructor_type->HasBase(*BuiltinTypes::ANY_TYPE)) {
        bool is_type = false;

        if (constructor_type == BuiltinTypes::CLASS_TYPE || constructor_type->HasBase(*BuiltinTypes::CLASS_TYPE)) {
            const AstExpression *value_of = m_proto->GetDeepValueOf(); // GetDeepValueOf() returns the non-wrapped generic AstTypeObject*
            const AstTypeObject *type_obj = nullptr;

            if (value_of != nullptr) {
                if (!(type_obj = dynamic_cast<const AstTypeObject *>(value_of))) {
                    if (const AstIdentifier *ident = dynamic_cast<const AstIdentifier *>(value_of)) {
                        type_obj = ident->ExtractTypeObject();
                    }
                }
            }

            if (type_obj != nullptr) {
                is_type = true;
                
                AssertThrow(type_obj->GetHeldType() != nullptr);
                m_symbol_type = type_obj->GetHeldType();

                SymbolMember_t proto_member;
                if (m_symbol_type->FindMember("$proto", proto_member)) {
                    m_prototype_type = std::get<1>(proto_member);

                    // only give default value IFF it is a built-in type
                    // this is to prevent huge prototypes being inlined.
                    if (m_prototype_type->GetTypeClass() == TYPE_BUILTIN) {
                        m_default_value = std::get<2>(proto_member);
                    }
                } else {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_type_missing_prototype,
                        m_location,
                        constructor_type->GetName()
                    ));
                }
            } else {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_not_a_constant_type,
                    m_location,
                    value_of != nullptr && value_of->GetExprType() != nullptr
                        ? value_of->GetExprType()->GetName()
                        : "<Cannot find value of>"
                ));
            }
        }
        
        if (!is_type) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location,
                constructor_type->GetName()
            ));
        }
    } else {
        // if not, it is a dynamic type
        m_default_value = BuiltinTypes::ANY->GetDefaultValue();
        m_symbol_type = BuiltinTypes::ANY;
        m_prototype_type = BuiltinTypes::ANY_TYPE;
    }
}

std::unique_ptr<Buildable> AstPrototypeSpecification::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_proto != nullptr);
    return m_proto->Build(visitor, mod);
}

void AstPrototypeSpecification::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_proto != nullptr);
    m_proto->Optimize(visitor, mod);
}

Pointer<AstStatement> AstPrototypeSpecification::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
