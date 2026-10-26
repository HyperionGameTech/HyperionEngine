/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorExtension.hpp>
#include <Editor/EditorSubsystem.hpp>


#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Logging/Logger.hpp>

#include <EditorExtension.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static Handle<EditorExtension> CreateEditorExtension(const Class* extensionClass)
{
    BoxedValue boxed;

    if (!extensionClass || !extensionClass->CreateInstance(boxed) || !boxed.Is<Handle<EditorExtension>>())
    {
        HYP_LOG(Editor, Error, "Failed to create editor extension {}", extensionClass ? extensionClass->GetName() : Name::Invalid());

        return Handle<EditorExtension>::empty;
    }

    return boxed.Get<Handle<EditorExtension>>();
}

EditorExtensionRegistry& EditorExtensionRegistry::GetInstance()
{
    static EditorExtensionRegistry s_instance;

    return s_instance;
}

uint32 EditorExtensionRegistry::Register(const Class* extensionClass)
{
    Handle<EditorExtension> attached;
    uint32 id = 0;

    {
        Mutex::Guard guard(m_mutex);

        id = m_nextId++;

        Entry& entry = m_entries.EmplaceBack();
        entry.id = id;
        entry.extensionClass = extensionClass;

        if (m_editorSubsystem != nullptr)
        {
            AttachEntry(entry);
            attached = entry.instance;
        }
    }

    if (attached.IsValid())
    {
        SyncSubsystem();
        attached->OnAttached();
    }

    return id;
}

void EditorExtensionRegistry::Unregister(uint32 id)
{
    Handle<EditorExtension> detached;

    {
        Mutex::Guard guard(m_mutex);

        auto it = m_entries.FindIf([id](const Entry& entry)
            {
                return entry.id == id;
            });

        if (it == m_entries.End())
        {
            return;
        }

        detached = it->instance;
        m_entries.Erase(it);
    }

    if (detached.IsValid())
    {
        detached->OnDetached();
        detached->m_editorSubsystem = nullptr;

        SyncSubsystem();
    }
}

void EditorExtensionRegistry::Attach(EditorSubsystem* editorSubsystem)
{
    Array<Handle<EditorExtension>> attached;

    {
        Mutex::Guard guard(m_mutex);

        m_editorSubsystem = editorSubsystem;

        for (Entry& entry : m_entries)
        {
            AttachEntry(entry);

            if (entry.instance.IsValid())
            {
                attached.PushBack(entry.instance);
            }
        }
    }

    SyncSubsystem();

    for (const Handle<EditorExtension>& extension : attached)
    {
        extension->OnAttached();
    }
}

void EditorExtensionRegistry::Detach()
{
    Array<Handle<EditorExtension>> detached;
    EditorSubsystem* editorSubsystem = nullptr;

    {
        Mutex::Guard guard(m_mutex);

        editorSubsystem = m_editorSubsystem;

        for (Entry& entry : m_entries)
        {
            if (entry.instance.IsValid())
            {
                detached.PushBack(std::move(entry.instance));
            }
        }

        m_editorSubsystem = nullptr;
    }

    for (auto it = detached.Begin(); it != detached.End(); ++it)
    {
        (*it)->OnDetached();
        (*it)->m_editorSubsystem = nullptr;
    }

    if (editorSubsystem != nullptr)
    {
        editorSubsystem->SetExtensions({});
    }
}

Array<Handle<EditorExtension>> EditorExtensionRegistry::GetExtensions() const
{
    Mutex::Guard guard(m_mutex);

    Array<Handle<EditorExtension>> extensions;
    extensions.Reserve(m_entries.Size());

    for (const Entry& entry : m_entries)
    {
        if (entry.instance.IsValid())
        {
            extensions.PushBack(entry.instance);
        }
    }

    return extensions;
}

void EditorExtensionRegistry::AttachEntry(Entry& entry)
{
    if (entry.instance.IsValid())
    {
        return;
    }

    entry.instance = CreateEditorExtension(entry.extensionClass);

    if (entry.instance.IsValid())
    {
        entry.instance->m_editorSubsystem = m_editorSubsystem;
    }
}

void EditorExtensionRegistry::SyncSubsystem()
{
    EditorSubsystem* editorSubsystem = nullptr;

    {
        Mutex::Guard guard(m_mutex);

        editorSubsystem = m_editorSubsystem;
    }

    if (editorSubsystem != nullptr)
    {
        editorSubsystem->SetExtensions(GetExtensions());
    }
}

} // namespace Hyperion
