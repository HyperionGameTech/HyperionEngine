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

class EditorSubsystem;
class EditorProject;

HYP_CLASS(Abstract)
class EditorActionBase : public HypObject<EditorActionBase>
{
    HYP_OBJECT_BODY(EditorActionBase);

public:
    virtual ~EditorActionBase() = default;

    HYP_METHOD(Scriptable)
    virtual Name GetName() const;

    HYP_METHOD(Scriptable)
    virtual void Execute(EditorSubsystem* editor_subsystem, EditorProject* project);

    HYP_METHOD(Scriptable)
    virtual void Revert(EditorSubsystem* editor_subsystem, EditorProject* project);

protected:
    virtual Name GetName_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void Execute_Impl(EditorSubsystem* editor_subsystem, EditorProject* project)
    {
        HYP_PURE_VIRTUAL();
    };

    virtual void Revert_Impl(EditorSubsystem* editor_subsystem, EditorProject* project)
    {
        HYP_PURE_VIRTUAL();
    }
};

struct EditorActionFunctions
{
    Proc<void(EditorSubsystem* editor_subsystem, EditorProject* project)> execute;
    Proc<void(EditorSubsystem* editor_subsystem, EditorProject* project)> revert;
};

HYP_CLASS()
class HYP_API FunctionalEditorAction : public EditorActionBase
{
    HYP_OBJECT_BODY(FunctionalEditorAction);

public:
    FunctionalEditorAction() = default;

    FunctionalEditorAction(Name name, Proc<EditorActionFunctions()>&& get_state_proc)
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
    virtual void Execute(EditorSubsystem* editor_subsystem, EditorProject* project) override final
    {
        m_get_state_proc_result.execute(editor_subsystem, project);
    }

    HYP_METHOD()
    virtual void Revert(EditorSubsystem* editor_subsystem, EditorProject* project) override final
    {
        m_get_state_proc_result.revert(editor_subsystem, project);
    }

private:
    Name m_name;
    Proc<EditorActionFunctions()> m_get_state_proc;
    EditorActionFunctions m_get_state_proc_result;
};

class IEditorActionFactory;

class HYP_API EditorActionFactoryRegistry
{
public:
    static EditorActionFactoryRegistry& GetInstance();

    IEditorActionFactory* GetFactoryByName(Name action_name) const;
    void RegisterFactory(Name action_name, IEditorActionFactory* factory);

private:
    HashMap<Name, IEditorActionFactory*> m_factories;
};

class IEditorActionFactory
{
public:
    virtual ~IEditorActionFactory() = default;

    virtual UniquePtr<EditorActionBase> CreateEditorActionInstance() const = 0;
};

template <class T>
class HYP_API EditorActionFactory final : public IEditorActionFactory
{
public:
    virtual ~EditorActionFactory() override = default;

    virtual UniquePtr<EditorActionBase> CreateEditorActionInstance() const override
    {
        return MakeUnique<T>();
    }
};

struct HYP_API EditorActionFactoryRegistrationBase
{
protected:
    IEditorActionFactory* m_factory;

    EditorActionFactoryRegistrationBase(Name action_name, IEditorActionFactory* factory);
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

} // namespace hyperion

#define HYP_DEFINE_EDITOR_ACTION(action_name)                                                                                    \
    class EditorAction_##action_name;                                                                                            \
    static ::hyperion::EditorActionFactoryRegistration<EditorAction_##action_name> EditorActionFactory_##action_name {}; \
    class EditorAction_##action_name : public ::hyperion::EditorAction

#endif
