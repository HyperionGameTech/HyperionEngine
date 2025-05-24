/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_ACTION_HPP
#define HYPERION_EDITOR_ACTION_HPP

#include <core/Name.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/Tuple.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_CLASS(Abstract)
class IEditorAction : public EnableRefCountedPtrFromThis<IEditorAction>
{
    HYP_OBJECT_BODY(IEditorAction);

public:
    virtual ~IEditorAction() = default;

    HYP_METHOD(Scriptable)
    virtual Name GetName() const;

    HYP_METHOD(Scriptable)
    virtual void Execute();

    HYP_METHOD(Scriptable)
    virtual void Revert();

protected:
    virtual Name GetName_Impl() const { HYP_PURE_VIRTUAL(); }
    virtual void Execute_Impl() { HYP_PURE_VIRTUAL(); };
    virtual void Revert_Impl() { HYP_PURE_VIRTUAL(); }
};

struct EditorActionFunctions
{
    Proc<void()>  execute;
    Proc<void()>  revert;
};

HYP_CLASS()
class HYP_API FunctionalEditorAction : public IEditorAction
{
    HYP_OBJECT_BODY(FunctionalEditorAction);

public:
    FunctionalEditorAction() = default;

    FunctionalEditorAction(Name name, Proc<EditorActionFunctions()> &&get_state_proc)
        : m_name(name),
          m_get_state_proc(std::move(get_state_proc)),
          m_get_state_proc_result(m_get_state_proc())
    {
    }

    HYP_METHOD()
    virtual Name GetName() const override final
    {
        return m_name;
    }

    HYP_METHOD()
    virtual void Execute() override final
    {
        m_get_state_proc_result.execute();
    }

    HYP_METHOD()
    virtual void Revert() override final
    {
        m_get_state_proc_result.revert();
    }

private:
    Name                            m_name;
    Proc<EditorActionFunctions()>   m_get_state_proc;
    EditorActionFunctions           m_get_state_proc_result;
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
} // namespace hyperion

#define HYP_DEFINE_EDITOR_ACTION(action_name) \
    class EditorAction_##action_name; \
    static ::hyperion::detail::EditorActionFactoryRegistration<EditorAction_##action_name> EditorActionFactory_##action_name { }; \
    class EditorAction_##action_name : public ::hyperion::EditorAction

#endif

