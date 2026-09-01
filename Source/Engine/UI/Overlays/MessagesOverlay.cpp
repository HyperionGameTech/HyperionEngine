/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/Overlays/MessagesOverlay.hpp>

#include <UI/UIListView.hpp>
#include <UI/UIText.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <MessagesOverlay.generated.inl>

namespace Hyperion {

#pragma region MessagesOverlay

MessagesOverlay::MessagesOverlay()
    : OverlayBase()
{
    // only update every second.
    m_timer = ClockTimer { 1.0f };
}

MessagesOverlay::~MessagesOverlay() = default;

void MessagesOverlay::PutMessage(MessageEntry&& entry)
{
    auto it = m_messages.Find(entry.key);
    if (it == m_messages.End())
    {
        m_messages.Insert(std::move(entry));

        return;
    }

    *it = entry;
}

void MessagesOverlay::ClearMessage(Name key)
{
    m_messages.Erase(key);
}

Handle<UIObject> MessagesOverlay::CreateUIObject(UIObject* spawnParent)
{
    m_listView = spawnParent->CreateUIObject<UIListView>(
        NAME("MessagesOverlay_List"),
        Vec2i::Zero(),
        UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));

    m_listView->SetTextSize(12.0f);
    m_listView->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.1f));

    return m_listView;
}

void MessagesOverlay::Update(float delta)
{
    HYP_SCOPE;

    if (!m_listView.IsValid())
    {
        return;
    }

    // tick down TTLs, dropping messages that have expired (ttl == 0 means "forever")
    const TimeDiff elapsed(int64(delta * 1000.0f));

    for (auto it = m_messages.Begin(); it != m_messages.End();)
    {
        if (it->ttl)
        {
            it->ttl -= elapsed;

            if (it->ttl <= TimeDiff(0))
            {
                it = m_messages.Erase(it);

                continue;
            }
        }

        ++it;
    }

    // drop messages for stuff that we no longer have
    bool childrenChanged = false;

    for (auto it = m_textElements.Begin(); it != m_textElements.End();)
    {
        if (!m_messages.Contains(it->first))
        {
            m_listView->RemoveChildUIObject(it->second.Get());

            it = m_textElements.Erase(it);

            childrenChanged = true;
        }
        else
        {
            ++it;
        }
    }

    // updates
    for (const MessageEntry& entry : m_messages)
    {
        auto elementIt = m_textElements.Find(entry.key);

        if (elementIt == m_textElements.End())
        {
            Handle<UIText> textElement = m_listView->CreateUIObject<UIText>(
                Vec2i::Zero(),
                UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));

            textElement->SetPadding(Vec2i { 2, 2 });

            m_listView->AddChildUIObject(textElement);

            elementIt = m_textElements.Set(entry.key, textElement).first;

            childrenChanged = true;
        }

        const Handle<UIText>& textElement = elementIt->second;

        textElement->SetText(entry.text);
        textElement->SetTextColor(entry.color);
    }

    m_listView->SetIsVisible(m_textElements.Any());
}

#pragma endregion MessagesOverlay

} // namespace Hyperion
