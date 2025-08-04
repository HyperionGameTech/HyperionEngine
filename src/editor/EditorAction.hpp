/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
class EditorActionBase : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorActionBase);

public:
    virtual ~EditorActionBase() = default;

    HYP_METHOD(Scriptable)
    virtual Name GetName() const;

    HYP_METHOD(Scriptable)
    virtual void Execute(EditorSubsystem* editorSubsystem, EditorProject* project);

    HYP_METHOD(Scriptable)
    virtual void Revert(EditorSubsystem* editorSubsystem, EditorProject* project);

protected:
    virtual Name GetName_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void Execute_Impl(EditorSubsystem* editorSubsystem, EditorProject* project)
    {
        HYP_PURE_VIRTUAL();
    };

    virtual void Revert_Impl(EditorSubsystem* editorSubsystem, EditorProject* project)
    {
        HYP_PURE_VIRTUAL();
    }
};

struct EditorActionFunctions
{
    Proc<void(EditorSubsystem* editorSubsystem, EditorProject* project)> execute;
    Proc<void(EditorSubsystem* editorSubsystem, EditorProject* project)> revert;
};

HYP_CLASS()
class HYP_API FunctionalEditorAction : public EditorActionBase
{
    HYP_OBJECT_BODY(FunctionalEditorAction);

public:
    FunctionalEditorAction() = default;

    FunctionalEditorAction(Name name, Proc<EditorActionFunctions()>&& getStateProc)
        : m_name(name),
          m_getStateProc(std::move(getStateProc)),
          m_getStateProcResult(m_getStateProc())
    {
    }

    HYP_METHOD()
    virtual Name GetName() const override final
    {
        return m_name;
    }

    HYP_METHOD()
    virtual void Execute(EditorSubsystem* editorSubsystem, EditorProject* project) override final
    {
        m_getStateProcResult.execute(editorSubsystem, project);
    }

    HYP_METHOD()
    virtual void Revert(EditorSubsystem* editorSubsystem, EditorProject* project) override final
    {
        m_getStateProcResult.revert(editorSubsystem, project);
    }

private:
    Name m_name;
    Proc<EditorActionFunctions()> m_getStateProc;
    EditorActionFunctions m_getStateProcResult;
};

class IEditorActionFactory;

class HYP_API EditorActionFactoryRegistry
{
public:
    static EditorActionFactoryRegistry& GetInstance();

    IEditorActionFactory* GetFactoryByName(Name actionName) const;
    void RegisterFactory(Name actionName, IEditorActionFactory* factory);

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

    EditorActionFactoryRegistrationBase(Name actionName, IEditorActionFactory* factory);
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

#define HYP_DEFINE_EDITOR_ACTION(actionName)                                                                                    \
    class EditorAction_##actionName;                                                                                            \
    static ::hyperion::EditorActionFactoryRegistration<EditorAction_##actionName> EditorActionFactory_##actionName {}; \
    class EditorAction_##actionName : public ::hyperion::EditorAction

