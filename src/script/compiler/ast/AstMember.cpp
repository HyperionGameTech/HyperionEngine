#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstMember::AstMember(
    const std::string &field_name,
    const std::shared_ptr<AstExpression> &target,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_field_name(field_name),
      m_target(target),
      m_symbol_type(BuiltinTypes::UNDEFINED),
      m_found_index(-1)
{
}

void AstMember::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    m_access_options = m_target->GetAccessOptions();

    m_target_type = m_target->GetExprType();
    AssertThrow(m_target_type != nullptr);

    const SymbolTypePtr_t original_type = m_target_type;

    // start looking at the target type,
    // iterate through base type
    SymbolTypePtr_t field_type = nullptr;

    while (field_type == nullptr && m_target_type != nullptr) {
        // allow boxing/unboxing
        if (m_target_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
            if (m_target_type->IsBoxedType()) {
                m_target_type = m_target_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
                AssertThrow(m_target_type != nullptr);
            }
        }

        if (m_target_type == BuiltinTypes::ANY) {
            field_type = BuiltinTypes::ANY;
            break;
        }

        if (SymbolTypePtr_t proto_type = m_target_type->FindMember("$proto")) {
            // get member index from name
            for (size_t i = 0; i < proto_type->GetMembers().size(); i++) {
                const SymbolMember_t &mem = proto_type->GetMembers()[i];

                if (std::get<0>(mem) == m_field_name) {
                    m_found_index = i;
                    field_type = std::get<1>(mem);
                    break;
                }
            }

            if (m_found_index != -1) {
                break;
            }
        }

        // for statics
        const AstExpression *value_of = m_target->GetValueOf();
        AssertThrow(value_of != nullptr);

        if (const AstTypeObject *as_type_object = dynamic_cast<const AstTypeObject *>(value_of)) {
            AssertThrow(as_type_object->GetHeldType() != nullptr);
            auto instance_type = as_type_object->GetHeldType();

            // get member index from name
            for (size_t i = 0; i < instance_type->GetMembers().size(); i++) {
                const SymbolMember_t &mem = instance_type->GetMembers()[i];

                if (std::get<0>(mem) == m_field_name) {
                    m_found_index = i;
                    field_type = std::get<1>(mem);

                    break;
                }
            }

            if (m_found_index != -1) {
                break;
            }
        }

        if (auto base = m_target_type->GetBaseType()) {
            m_target_type = base;
        } else {
            break;
        }

        // if (field_type == nullptr) {
        //     // look for members in base class
        //     m_target_type = m_target_type->GetBaseType();
        // } else {
        //     break;
        // }
    }

    AssertThrow(m_target_type != nullptr);

    if (field_type != nullptr) {
        m_symbol_type = field_type;
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_data_member,
            m_location,
            m_field_name,
            original_type->GetName()
        ));
    }
}

std::unique_ptr<Buildable> AstMember::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_target != nullptr);
    chunk->Append(m_target->Build(visitor, mod));

    AssertThrow(m_target_type != nullptr);

    if (m_target_type == BuiltinTypes::ANY) {
        // for Any type we will have to load from hash
        const uint32_t hash = hash_fnv_1(m_field_name.c_str());

        switch (m_access_mode) {
            case ACCESS_MODE_LOAD:
                chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
                break;
            case ACCESS_MODE_STORE:
                chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
                break;
        }
    } else {
        AssertThrow(m_found_index != -1);

        switch (m_access_mode) {
            case ACCESS_MODE_LOAD:
                // just load the data member.
                chunk->Append(Compiler::LoadMemberAtIndex(
                    visitor,
                    mod,
                    m_found_index
                ));
                break;
            case ACCESS_MODE_STORE:
                // we are in storing mode, so store to LAST item in the member expr.
                chunk->Append(Compiler::StoreMemberAtIndex(
                    visitor,
                    mod,
                    m_found_index
                ));
                break;
        }
    }

    return std::move(chunk);
}

void AstMember::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);

    // TODO: check if the member being accessed is constant and can
    // be optimized
}

Pointer<AstStatement> AstMember::Clone() const
{
    return CloneImpl();
}

Tribool AstMember::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstMember::MayHaveSideEffects() const
{
    return false;
}

SymbolTypePtr_t AstMember::GetExprType() const
{
    AssertThrow(m_symbol_type != nullptr);
    return m_symbol_type;
}

const AstExpression *AstMember::GetValueOf() const
{
    // if (m_symbol_type != nullptr && m_symbol_type->GetDefaultValue() != nullptr) {
    //     return m_symbol_type->GetDefaultValue()->GetValueOf();
    // }

    return AstExpression::GetValueOf();
}

const AstExpression *AstMember::GetDeepValueOf() const
{
    // if (m_symbol_type != nullptr && m_symbol_type->GetDefaultValue() != nullptr) {
    //     return m_symbol_type->GetDefaultValue()->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

} // namespace hyperion::compiler
