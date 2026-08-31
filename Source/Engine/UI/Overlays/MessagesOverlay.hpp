/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <UI/Overlays/Overlay.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/Time.hpp>

namespace Hyperion {

class UIListView;
class UIText;

struct MessageEntry
{
    Name key;
    String text;
    Color color;
    TimeDiff ttl = TimeDiff(0); // forever
};

HYP_CLASS(NoScriptBindings)
class ENGINE_API MessagesOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(MessagesOverlay);

public:
    MessagesOverlay();
    virtual ~MessagesOverlay() override;

    void PutMessage(MessageEntry&& entry);
    void ClearMessage(Name key);

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 2; // top-right
    }

    virtual void Update(float delta) override;

private:
    Handle<UIListView> m_listView;
    Map<Name, Handle<UIText>> m_textElements;

    HashTable<MessageEntry, &MessageEntry::key> m_messages;
};

} // namespace Hyperion
