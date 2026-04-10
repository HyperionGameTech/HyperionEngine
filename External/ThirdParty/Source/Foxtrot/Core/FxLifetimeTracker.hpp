#pragma once

#include <Core/FxBitset.hpp>
#include <Core/FxLog.hpp>

#define FX_LIFETIME_MARK_CREATED(tracker_object_)   tracker_object_.MarkCreated(__func__)
#define FX_LIFETIME_MARK_DESTROYED(tracker_object_) tracker_object_.MarkDestroyed(__func__)

class FxLifetimeTracker
{
public:
    void MarkCreated(const char* creator_name)
    {
        bIsActive = true;
        CreatorName = creator_name;

        FxLogDebug("LifetimeTracker: Object '{}' has been created by '{}'", Name, creator_name);
    }

    void SetName(const char* name) { Name = name; }

    void MarkDestroyed(const char* destroyer_name)
    {
        bIsActive = false;
        DestroyerName = destroyer_name;

        FxLogDebug("LifetimeTracker: Object '{}' has been created by '{}'", Name, destroyer_name);
    };

public:
    const char* Name = "Unknown";

    const char* DestroyerName = "None";
    const char* CreatorName = "None";

    bool bIsActive = false;
};
