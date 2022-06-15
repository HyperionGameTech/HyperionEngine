#include <script/compiler/ast/AstNewExpression.hpp>
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

AstNewExpression::AstNewExpression(
    const std::shared_ptr<AstPrototypeSpecification> &proto,
    const std::shared_ptr<AstArgumentList> &arg_list,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_proto(proto),
      m_arg_list(arg_list),
      m_is_dynamic_type(false)
{
}

void AstNewExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_proto != nullptr);
    m_proto->Visit(visitor, mod);

    /*AssertThrow(m_proto->GetExprType() != nullptr);
    m_constructor_type = m_proto->GetExprType();

    const bool is_type = m_constructor_type == BuiltinTypes::CLASS_TYPE;*/
    
    m_instance_type = BuiltinTypes::UNDEFINED;
    m_prototype_type = BuiltinTypes::UNDEFINED;

    if (const auto &held_type = m_proto->GetHeldType()) {
        m_instance_type = held_type;
        m_object_value = m_proto->GetDefaultValue(); // may be nullptr
        m_prototype_type = m_proto->GetPrototypeType();
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            m_proto->GetExprType() != nullptr
                ? m_proto->GetExprType()->GetName()
                : "??"
        ));
    }

    /*BuiltinTypes::ANY;

    if (m_constructor_type != BuiltinTypes::ANY) {
        if (const AstIdentifier *as_ident = dynamic_cast<AstIdentifier*>(m_proto.get())) {
            if (const auto current_value = as_ident->GetProperties().GetIdentifier()->GetCurrentValue()) {
                if (AstTypeObject *type_object = dynamic_cast<AstTypeObject*>(current_value.get())) {
                    AssertThrow(type_object->GetHeldType() != nullptr);
                    m_instance_type = type_object->GetHeldType();

                    SymbolMember_t proto_member;
                    if (type_object->GetHeldType()->FindMember("$proto", proto_member)) {
                        m_object_value = std::get<2>(proto_member); // NOTE: may be null causing NEW operand to be emitted
                    }
                } else if (!is_type) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_not_a_type,
                        m_location
                    ));
                }
            }
        } else if (!is_type) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location
            ));
        }
    }*/

    // get default value
    /*if (m_object_type != nullptr) {
        if (auto object_value = m_object_type->GetDefaultValue()) {
            bool should_call_constructor = false;

            //if (object_type != BuiltinTypes::ANY) {
                const bool has_written_constructor = m_object_type->FindMember("new") != nullptr;
                const bool has_args = m_arg_list != nullptr && !m_arg_list->GetArguments().empty();

                if (has_written_constructor || has_args) {
                    should_call_constructor = true;
                }
            //}

            if (should_call_constructor) {
                std::vector<std::shared_ptr<AstArgument>> args;

                if (m_arg_list != nullptr) {
                    args = m_arg_list->GetArguments();
                }

                m_constructor_call.reset(new AstCallExpression(
                    std::shared_ptr<AstMember>(new AstMember(
                        "new",
                        object_value,
                        m_location
                    )),
                    args,
                    true,
                    m_location
                ));

                m_constructor_call->Visit(visitor, mod);
            }

            m_object_value = object_value;
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_type_no_default_assignment,
                m_location,
                m_object_type->GetName()
            ));
        }
    } else {
        // TODO: runtime constructor check?
        // part of the upcoming runtime-typechecking feature
        m_is_dynamic_type = true;
    }*/
}

std::unique_ptr<Buildable> AstNewExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_prototype_type != nullptr);

#if ACE_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE
    // does not currently work in templates
    // e.g `new X` where `X` is `String` as a template argument, attempts to
    // construct the object rather than baking in
    if (m_object_value != nullptr && m_prototype_type->GetTypeClass() == TYPE_BUILTIN) {
        chunk->Append(m_object_value->Build(visitor, mod));
    } else {
#endif
        AssertThrow(m_proto != nullptr);
        chunk->Append(m_proto->Build(visitor, mod));

        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instr_new = BytecodeUtil::Make<RawOperation<>>();
        instr_new->opcode = NEW;
        instr_new->Accept<uint8_t>(rp); // dst (overwrite proto)
        instr_new->Accept<uint8_t>(rp); // src (holds proto)
        chunk->Append(std::move(instr_new));
#if ACE_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE
    }
#endif
    
    
    /*AssertThrow(m_proto != nullptr);
    chunk->Append(m_proto->Build(visitor, mod));

    if (m_is_dynamic_type) {
        // register holding the main object
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instr_new = BytecodeUtil::Make<RawOperation<>>();
        instr_new->opcode = NEW;
        instr_new->Accept<uint8_t>(rp); // dst (overwrite proto)
        instr_new->Accept<uint8_t>(rp); // src (holds proto)
        chunk->Append(std::move(instr_new));
    } else {
        if (m_constructor_call != nullptr) {
            chunk->Append(m_constructor_call->Build(visitor, mod));
        } else {
            // build in the value
            AssertThrow(m_object_value != nullptr);
            chunk->Append(m_object_value->Build(visitor, mod));
        }
    }*/

    return std::move(chunk);
}

void AstNewExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    // AssertThrow(m_type_expr != nullptr);
    // m_type_expr->Optimize(visitor, mod);
    AssertThrow(m_proto != nullptr);
    m_proto->Optimize(visitor, mod);

    if (m_object_value != nullptr) {
        m_object_value->Optimize(visitor, mod);
    }

    /*if (!m_is_dynamic_type) {
        if (m_constructor_call != nullptr) {
            m_constructor_call->Optimize(visitor, mod);
        } else {
            // build in the value
            AssertThrow(m_object_value != nullptr);
            m_object_value->Optimize(visitor, mod);
        }
    }*/
}

Pointer<AstStatement> AstNewExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstNewExpression::IsTrue() const
{
    if (m_object_value != nullptr) {
        return m_object_value->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstNewExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstNewExpression::GetExprType() const
{
    AssertThrow(m_instance_type != nullptr);
    return m_instance_type;
    // AssertThrow(m_type_expr != nullptr);
    // AssertThrow(m_type_expr->GetSpecifiedType() != nullptr);

    // return m_type_expr->GetSpecifiedType();
}

} // namespace hyperion::compiler
