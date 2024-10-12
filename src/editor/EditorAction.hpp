/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_ACTION_HPP
#define HYPERION_EDITOR_ACTION_HPP

#include <core/Name.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/utilities/Tuple.hpp>

#include <core/containers/TypeMap.hpp>

#include <Types.hpp>

namespace hyperion {
namespace editor {

class IEditorAction
{
public:
    virtual ~IEditorAction() = default;

    virtual Name GetName() const = 0;

    virtual void Execute() = 0;

    virtual void Undo() = 0;
    virtual void Redo() = 0;
};

template <auto NameStaticString, class... Args>
class HYP_API EditorAction : public IEditorAction
{
public:
    static Name GetName_Static()
    {
        static const Name name = CreateNameFromDynamicString(NameStaticString.Data());

        return name;
    }

    EditorAction()
        : m_is_executed(false)
    {
    }

    virtual ~EditorAction() override = default;

    virtual Name GetName() const final override
    {
        return GetName_Static();
    }

    virtual void Execute() final
    {
        if (m_is_executed) {
            return;
        }

        Execute_Internal();

        m_is_executed = true;
    }

    virtual void Undo() final override
    {
        if (!m_is_executed) {
            return;
        }

        Undo_Internal();

        m_is_executed = false;
        m_args = { };
    }

    virtual void Redo() final override
    {
        if (m_is_executed) {
            return;
        }

        Execute_Internal();
        m_is_executed = true;
    }

protected:
    virtual void Execute_Internal() = 0;
    virtual void Undo_Internal() = 0;

private:
    bool                            m_is_executed;
    Tuple<NormalizedType<Args>...>  m_args;
};

class IEditorActionFactory;

class HYP_API EditorActionFactoryRegistry
{
public:
    static EditorActionFactoryRegistry &GetInstance();

    IEditorActionFactory *GetFactoryByName(Name action_name) const;
    void RegisterFactory(Name action_name, IEditorActionFactory *factory);

private:
    HashMap<Name, IEditorActionFactory *>   m_factories;
};

class IEditorActionFactory
{
public:
    virtual ~IEditorActionFactory() = default;

    virtual UniquePtr<IEditorAction> CreateEditorActionInstance() const = 0;
};

template <class T>
class HYP_API EditorActionFactory final : public IEditorActionFactory
{
public:
    virtual ~EditorActionFactory() override = default;

    virtual UniquePtr<IEditorAction> CreateEditorActionInstance() const override
    {
        return MakeUnique<T>();
    }
};

namespace detail {

struct HYP_API EditorActionFactoryRegistrationBase
{
protected:
    IEditorActionFactory    *factory;

    EditorActionFactoryRegistrationBase(Name action_name, IEditorActionFactory *factory);
    ~EditorActionFactoryRegistrationBase();
};

template <class EditorActionClass>
struct EditorActionFactoryRegistration : public EditorActionFactoryRegistrationBase
{
    EditorActionFactoryRegistration()
        : EditorActionFactoryRegistrationBase(EditorActionClass::GetName_Static(), new EditorActionFactory<EditorActionClass>())
    {
    }
};

} // namespace detail
} // namespace editor
} // namespace hyperion

#define HYP_DEFINE_EDITOR_ACTION(action_name, ...) \
    class EditorAction_##action_name; \
    static ::hyperion::editor::detail::EditorActionFactoryRegistration<EditorAction_##action_name> EditorActionFactory_##action_name { }; \
    class EditorAction_##action_name : public ::hyperion::editor::EditorAction<HYP_STATIC_STRING(HYP_STR(action_name)), ##__VA_ARGS__>

#endif

