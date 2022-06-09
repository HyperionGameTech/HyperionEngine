#include <script/compiler/ast/AstTypeDefinition.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstObject.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Hasher.hpp>
#include <system/debug.h>

namespace hyperion::compiler {

AstTypeDefinition::AstTypeDefinition(const std::string &name,
    const std::vector<std::string> &generic_params,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
    const std::vector<std::shared_ptr<AstEvent>> &events,
    const SourceLocation &location)
    : AstStatement(location),
      m_name(name),
      m_generic_params(generic_params),
      m_members(members),
      m_events(events),
      m_num_members(0)
{
}

void AstTypeDefinition::Visit(AstVisitor *visitor, Module *mod)
{   
    AssertThrow(visitor != nullptr && mod != nullptr);

    if (mod->LookupSymbolType(m_name)) {
        // error; redeclaration of type in module
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_redefined_type,
            m_location,
            m_name
        ));
    } else {
        unsigned int depth = 0;

        TreeNode<Scope> *top = mod->m_scopes.TopNode();
        while (top != nullptr) {
            top = top->m_parent;
            depth++;
        }

        // if (depth > 1) {
        //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        //         LEVEL_ERROR,
        //         Msg_type_not_defined_globally,
        //         m_location
        //     ));
        // } else {
            // open the scope for data members
            mod->m_scopes.Open(Scope(SCOPE_TYPE_TYPE_DEFINITION, 0));

            // handle generic parameter declarations
            std::vector<SymbolTypePtr_t> generic_param_types;

            const bool is_generic = !m_generic_params.empty();

            for (const std::string &generic_name : m_generic_params) {
                auto it = std::find_if(
                    generic_param_types.begin(),
                    generic_param_types.end(),
                    [&generic_name](SymbolTypePtr_t &item) {
                        return item->GetName() == generic_name;
                    }
                );

                if (it == generic_param_types.end()) {
                    SymbolTypePtr_t type = SymbolType::GenericParameter(
                        generic_name, 
                        nullptr /* substitution is nullptr because this is not a generic instance*/
                    );

                    generic_param_types.push_back(type);
                    mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(type);
                } else {
                    // error; redeclaration of generic parameter
                    visitor->GetCompilationUnit()->GetErrorList().AddError(
                        CompilerError(
                            LEVEL_ERROR,
                            Msg_generic_parameter_redeclared,
                            m_location,
                            generic_name
                        )
                    );
                }
            }

            if (!m_events.empty()) {
                std::vector<std::shared_ptr<AstExpression>> event_items;
                // each event item is an array of size 2 (could be tuple in the future?)
                for (size_t i = 0; i < m_events.size(); i++) {
                    const std::shared_ptr<AstEvent> &event = m_events[i];
                    if (event != nullptr) {
                        //event->Visit(visitor, mod);
                        event_items.push_back(std::shared_ptr<AstArrayExpression>(new AstArrayExpression(
                            { event->GetKey(), event->GetTrigger() },
                            m_location
                        )));
                    }
                }

                // builtin members:
                m_members.push_back(std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
                    "$events",
                    nullptr,
                    std::shared_ptr<AstArrayExpression>(new AstArrayExpression(
                        event_items,
                        m_location
                    )),
                    {},
                    false, // not const
                    false, // not generic
                    m_location
                )));
            }

            std::vector<SymbolMember_t> member_types;

            for (const auto &mem : m_members) {
                if (mem != nullptr) {
                    mem->Visit(visitor, mod);

                    AssertThrow(mem->GetIdentifier() != nullptr);

                    std::string mem_name = mem->GetName();
                    SymbolTypePtr_t mem_type = mem->GetIdentifier()->GetSymbolType();
                    
                    member_types.push_back(std::make_tuple(
                        mem_name,
                        mem_type,
                        mem->GetRealAssignment()
                    ));
                }
            }

            // close the scope for data members
            mod->m_scopes.Close();

            SymbolTypePtr_t symbol_type;

            if (!is_generic) {
                symbol_type = SymbolType::Object(
                    m_name,
                    member_types
                );
            } else {
                symbol_type = SymbolType::Generic(
                    m_name,
                    member_types, 
                    GenericTypeInfo {
                        (int)m_generic_params.size(),
                        generic_param_types
                    },
                    BuiltinTypes::OBJECT
                );

                symbol_type->SetDefaultValue(std::shared_ptr<AstObject>(
                    new AstObject(symbol_type, SourceLocation::eof)
                ));
            }

            // register the main type
            visitor->GetCompilationUnit()->RegisterType(symbol_type);

            // add the type to the identifier table, so it's usable
            Scope &top_scope = mod->m_scopes.Top();
            top_scope.GetIdentifierTable().AddSymbolType(symbol_type);
        //}
    }
}

std::unique_ptr<Buildable> AstTypeDefinition::Build(AstVisitor *visitor, Module *mod)
{
    return nullptr;
}

void AstTypeDefinition::Optimize(AstVisitor *visitor, Module *mod)
{
}

Pointer<AstStatement> AstTypeDefinition::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
