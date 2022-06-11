#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/debug.h>
#include <util/utf8.hpp>

namespace hyperion::compiler {

AstTypeSpecification::AstTypeSpecification(
    const std::string &left,
    const std::vector<std::shared_ptr<AstTypeSpecification>> &generic_params,
    const std::shared_ptr<AstTypeSpecification> &right,
    const SourceLocation &location)
    : AstStatement(location),
      m_left(left),
      m_generic_params(generic_params),
      m_right(right),
      m_symbol_type(BuiltinTypes::UNDEFINED),
      m_original_type(BuiltinTypes::UNDEFINED),
      m_is_chained(false)
{
}

void AstTypeSpecification::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr && mod != nullptr);

    std::vector<GenericInstanceTypeInfo::Arg> generic_types;

    for (auto &param : m_generic_params) {
        if (param != nullptr) {
            param->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());

            AssertThrow(param->GetSpecifiedType() != nullptr);
            generic_types.push_back({
                "", param->GetSpecifiedType()
            });
        }
    }

    if (m_right == nullptr) {
        AstVariable left_var(m_left, m_location);
        left_var.Visit(visitor, mod);

        if (left_var.GetProperties().GetIdentifier() != nullptr) {
            const SymbolTypePtr_t expr_type = left_var.GetExprType();
            SymbolTypePtr_t symbol_type = nullptr;

            // attempt to extract the held SymbolType out of an identifier's value
            if (const auto current_value = left_var.GetProperties().GetIdentifier()->GetCurrentValue()) {
                if (AstTypeObject *type_object = dynamic_cast<AstTypeObject*>(current_value.get())) {
                    AssertThrow(type_object->GetHeldType() != nullptr);
                    symbol_type = type_object->GetHeldType();
                }
            }

            //SymbolTypePtr_t symbol_type = left_var.GetExprType();//mod->LookupSymbolType(m_left);
            //AssertThrow(symbol_type != nullptr);

            if (expr_type == BuiltinTypes::ANY || (expr_type == BuiltinTypes::CLASS_TYPE && symbol_type == nullptr)) {
                // ???
                m_symbol_type = BuiltinTypes::ANY;
            } else if (!symbol_type->IsOrHasBase(*BuiltinTypes::CLASS_TYPE) && !symbol_type->IsOrHasBase(*BuiltinTypes::TRAIT_TYPE)) {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_not_a_type,
                    m_location,
                    symbol_type->GetName()
                ));
            } else {
                // get type of the '$proto' member, which is an instance type
                // if (SymbolTypePtr_t proto_member_type = symbol_type->FindMember("$proto")) {
                //     symbol_type = proto_member_type;
                // }

                m_original_type = symbol_type;

                // if the type is an alias, get the aliasee
                symbol_type = symbol_type->GetUnaliased();

                switch (symbol_type->GetTypeClass()) {
                    case TYPE_GENERIC_PARAMETER:
                        // if it is a generic parameter:
                        //   if the substitution has been supplied:
                        //     set the type to be that.
                        //   else:
                        //     set the type to be the param itself.
                        if (auto substitution_type = symbol_type->GetGenericParameterInfo().m_substitution.lock()) {
                            // set type to be substituted type
                            m_symbol_type = substitution_type;
                        } else {
                            m_symbol_type = symbol_type;
                        }

                        break;
                    
                    case TYPE_GENERIC:
                        // check generic params
                        if (!m_generic_params.empty()) {
                            // look up generic instance to see if it's already been created
                            m_symbol_type = visitor->GetCompilationUnit()->
                                GetCurrentModule()->LookupGenericInstance(symbol_type, generic_types);

                            if (m_symbol_type == nullptr) {
                                // nothing found from lookup,
                                // so create new generic instance
                                const bool num_provided_parameters_matches =
                                    symbol_type->GetGenericInfo().m_num_parameters == generic_types.size();

                                const bool no_parameters_required = symbol_type->GetGenericInfo().m_num_parameters == -1;

                                const bool valid_parameters = no_parameters_required || num_provided_parameters_matches;

                                if (valid_parameters) {
                                    // open the scope for data members
                                    mod->m_scopes.Open(Scope());

                                    // for each supplied parameter, create substitution
                                    for (size_t i = 0; 
                                        i < generic_types.size() && i < symbol_type->GetGenericInfo().m_params.size(); i++)
                                    {
                                        const SymbolTypePtr_t &aliasee_type = generic_types[i].m_type;
                                        AssertThrow(aliasee_type != nullptr);

                                        // create alias type
                                        SymbolTypePtr_t alias_type = SymbolType::Alias(
                                            symbol_type->GetGenericInfo().m_params[i]->GetName(),
                                            { aliasee_type }
                                        );

                                        // add it
                                        visitor->GetCompilationUnit()->GetCurrentModule()->
                                            m_scopes.Top().GetIdentifierTable().AddSymbolType(alias_type);
                                    }

                                    SymbolTypePtr_t new_instance = SymbolType::GenericInstance(
                                        symbol_type,
                                        GenericInstanceTypeInfo {
                                            generic_types
                                        }
                                    );

                                    // accept all members
                                    for (auto &mem : new_instance->GetMembers()) {
                                        SymbolTypePtr_t &mem_symbol_type = std::get<1>(mem);
                                        std::shared_ptr<AstExpression> &mem_assignment = std::get<2>(mem);

                                        for (size_t i = 0; i < generic_types.size() && i < symbol_type->GetGenericInfo().m_params.size(); i++)
                                        {
                                            if (const SymbolTypePtr_t &placeholder = symbol_type->GetGenericInfo().m_params[i]) {
                                                if (const SymbolTypePtr_t &substitute = generic_types[i].m_type) {
                                                    mem_symbol_type = SymbolType::SubstituteGenericParams(
                                                        mem_symbol_type,
                                                        placeholder,
                                                        substitute
                                                    );
                                                }
                                            }
                                        }

                                        AssertThrow(mem_symbol_type != nullptr);

                                        if (mem_assignment == nullptr) {
                                            // set to default value of symbol type if assignment not given
                                            AssertThrow(mem_symbol_type->GetDefaultValue() != nullptr);
                                            mem_assignment = mem_symbol_type->GetDefaultValue();
                                        }

                                        // accept assignment for new member instance
                                        mem_assignment->Visit(visitor, mod);
                                        
                                        SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
                                            visitor,
                                            mod,
                                            mem_symbol_type,
                                            mem_assignment->GetExprType(),
                                            mem_assignment->GetLocation()
                                        );
                                    }

                                    // close the scope for data members
                                    mod->m_scopes.Close();

                                    // allow generic instance to be used in code
                                    visitor->GetCompilationUnit()->GetCurrentModule()->
                                        m_scopes.Root().GetIdentifierTable().AddSymbolType(new_instance);
                                    
                                    if (!new_instance->GetMembers().empty()) {
                                        new_instance->SetDefaultValue(std::shared_ptr<AstObject>(
                                            new AstObject(new_instance, SourceLocation::eof)
                                        ));
                                    }

                                    m_symbol_type = new_instance;
                                } else {
                                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                                        LEVEL_ERROR,
                                        Msg_generic_parameters_missing,
                                        m_location,
                                        symbol_type->GetName(),
                                        symbol_type->GetGenericInfo().m_num_parameters
                                    ));

                                    m_symbol_type = symbol_type;
                                }
                            }
                        } else {
                            m_symbol_type = symbol_type;
                        }

                        break;

                    default:
                        m_symbol_type = symbol_type;

                        if (!m_generic_params.empty()) {
                            // not a generic type but generic params supplied
                            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                                LEVEL_ERROR,
                                Msg_type_not_generic,
                                m_location,
                                symbol_type->GetName()
                            ));
                        }

                        break;
                }
            }
        }
    } else {
        Module *left_mod = nullptr;

        if (m_is_chained) {
            AssertThrow(mod != nullptr);
            AssertThrow(mod->GetImportTreeLink() != nullptr);

            // search siblings of the current module,
            // rather than global lookup.
            for (auto *sibling : mod->GetImportTreeLink()->m_siblings) {
                AssertThrow(sibling != nullptr);
                AssertThrow(sibling->m_value != nullptr);

                if (sibling->m_value->GetName() == m_left) {
                    left_mod = sibling->m_value;
                }
            }
        } else {
            left_mod = visitor->GetCompilationUnit()->LookupModule(m_left);
        }

        if (m_right->m_right != nullptr) {
            // set next to chained
            m_right->m_is_chained = true;
        }

        // module access type
        if (left_mod != nullptr) {
            // accept the right-hand side
            m_right->Visit(visitor, left_mod);
            AssertThrow(m_right->GetSpecifiedType() != nullptr);

            m_symbol_type = m_right->GetSpecifiedType();
        } else {
            // did not find module
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unknown_module,
                m_location,
                m_left
            ));
        }
    }
}

std::unique_ptr<Buildable> AstTypeSpecification::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstTypeSpecification::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstTypeSpecification::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
