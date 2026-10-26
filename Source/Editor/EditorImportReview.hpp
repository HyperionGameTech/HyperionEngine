/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class EditorSubsystem;

HYP_CLASS(Abstract, Serialize = false)
class EDITOR_API EditorImportReview : public ObjectBase
{
    HYP_OBJECT_BODY(EditorImportReview);

public:
    ~EditorImportReview() override = default;

    HYP_METHOD()
    virtual String GetFileName() const = 0;

    HYP_METHOD()
    virtual bool Commit(EditorSubsystem* subsystem) = 0;

    HYP_METHOD()
    virtual void Cancel() = 0;

    HYP_METHOD()
    bool IsFinished() const
    {
        return m_isFinished;
    }

    HYP_METHOD()
    uint32 NumSettings() const
    {
        return uint32(m_settings.Size());
    }

    HYP_METHOD()
    Handle<ObjectBase> GetSettings(uint32 index) const
    {
        return index < m_settings.Size() ? m_settings[index] : Handle<ObjectBase>::empty;
    }

protected:
    Array<Handle<ObjectBase>> m_settings;
    bool m_isFinished = false;
};

} // namespace Hyperion
