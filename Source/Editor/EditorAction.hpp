/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Editor/EditorMemory.hpp>
namespace Hyperion {

class EditorSubsystem;
class EditorProject;

HYP_CLASS(Abstract)
class EDITOR_API EditorActionBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorActionBase);

public:
    static Pool* GetAllocator() { return g_editorPool; }

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
class EDITOR_API FunctionalEditorAction : public EditorActionBase
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

} // namespace Hyperion
