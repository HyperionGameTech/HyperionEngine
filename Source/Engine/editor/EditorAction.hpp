/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/name/Name.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorProject;

HYP_CLASS(Abstract)
class EditorActionBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorActionBase);

public:
    virtual ~EditorActionBase() = default;

    HYP_METHOD(Scriptable)
    virtual String GetText() const;

    HYP_METHOD(Scriptable)
    virtual void Execute(EditorSubsystem* editorSubsystem, EditorProject* project);

    HYP_METHOD(Scriptable)
    virtual void Revert(EditorSubsystem* editorSubsystem, EditorProject* project);

protected:
    virtual String GetText_Impl() const
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

    FunctionalEditorAction(const String& text, Proc<EditorActionFunctions()>&& getStateProc)
        : m_text(text),
          m_getStateProc(std::move(getStateProc)),
          m_getStateProcResult(m_getStateProc())
    {
    }

    HYP_METHOD()
    virtual String GetText() const override final
    {
        return m_text;
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
    String m_text;
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
    TMap<Name, IEditorActionFactory*> m_factories;
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

} // namespace Hyperion

#define HYP_DEFINE_EDITOR_ACTION(actionName)                                                                           \
    class EditorAction_##actionName;                                                                                   \
    static ::Hyperion::EditorActionFactoryRegistration<EditorAction_##actionName> EditorActionFactory_##actionName {}; \
    class EditorAction_##actionName : public ::Hyperion::EditorAction
