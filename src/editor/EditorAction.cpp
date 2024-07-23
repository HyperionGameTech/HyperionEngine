/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorAction.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace editor {

#pragma region EditorActionFactoryRegistry

EditorActionFactoryRegistry &EditorActionFactoryRegistry::GetInstance()
{
    static EditorActionFactoryRegistry instance { };

    return instance;
}

IEditorActionFactory *EditorActionFactoryRegistry::GetFactoryByName(Name action_name) const
{
    const auto it = m_factories.Find(action_name);

    if (it == m_factories.End()) {
        return nullptr;
    }

    return it->second;
}

void EditorActionFactoryRegistry::RegisterFactory(Name action_name, IEditorActionFactory *factory)
{
    auto it = m_factories.Find(action_name);
    AssertThrowMsg(it == m_factories.End(), "Editor action factory with name %s already registered", action_name.LookupString());

    m_factories.Set(action_name, factory);
}

#pragma endregion EditorActionFactoryRegistry

#pragma region EditorActionFactoryRegistrationBase

namespace detail {
    
EditorActionFactoryRegistrationBase::EditorActionFactoryRegistrationBase(Name action_name, IEditorActionFactory *factory)
    : factory(factory)
{
    EditorActionFactoryRegistry::GetInstance().RegisterFactory(action_name, factory);
}

EditorActionFactoryRegistrationBase::~EditorActionFactoryRegistrationBase()
{
    delete factory;
}

} // namespace detail

#pragma endregion EditorActionFactoryRegistrationBase

#pragma region Default Editor Actions

HYP_DEFINE_EDITOR_ACTION(GenerateLightmaps)
{
    virtual void Execute_Internal() override
    {
        HYP_LOG(Editor, LogLevel::DEBUG, "Generate lightmaps - not yet implemented");
    }

    virtual void Undo_Internal() override
    {
        HYP_LOG(Editor, LogLevel::DEBUG, "Undo generate lightmaps - not yet implemented");
    }
};

#pragma endregion Default Editor Actions

} // namespace editor
} // namespace hyperion