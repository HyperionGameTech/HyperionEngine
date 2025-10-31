/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorAction.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorSubsystem.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EditorAction.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorActionFactoryRegistry

EditorActionFactoryRegistry& EditorActionFactoryRegistry::GetInstance()
{
    static EditorActionFactoryRegistry s_instance {};

    return s_instance;
}

IEditorActionFactory* EditorActionFactoryRegistry::GetFactoryByName(Name actionName) const
{
    const auto it = m_factories.Find(actionName);

    if (it == m_factories.End())
    {
        return nullptr;
    }

    return it->second;
}

void EditorActionFactoryRegistry::RegisterFactory(Name actionName, IEditorActionFactory* factory)
{
    auto it = m_factories.Find(actionName);
    Assert(it == m_factories.End(), "Editor action factory with name %s already registered", actionName.LookupString());

    m_factories.Set(actionName, factory);
}

#pragma endregion EditorActionFactoryRegistry

#pragma region EditorActionFactoryRegistrationBase

EditorActionFactoryRegistrationBase::EditorActionFactoryRegistrationBase(Name actionName, IEditorActionFactory* factory)
    : m_factory(factory)
{
    EditorActionFactoryRegistry::GetInstance().RegisterFactory(actionName, factory);
}

EditorActionFactoryRegistrationBase::~EditorActionFactoryRegistrationBase()
{
    delete m_factory;
}

#pragma endregion EditorActionFactoryRegistrationBase

#pragma region Default Editor Actions

// HYP_DEFINE_EDITOR_ACTION(GenerateLightmaps)
// {
//     virtual void Execute_Internal() override
//     {
//         HYP_LOG(Editor, Debug, "Generate lightmaps - not yet implemented");
//     }

//     virtual void Undo_Internal() override
//     {
//         HYP_LOG(Editor, Debug, "Undo generate lightmaps - not yet implemented");
//     }
// };

#pragma endregion Default Editor Actions

} // namespace hyperion
