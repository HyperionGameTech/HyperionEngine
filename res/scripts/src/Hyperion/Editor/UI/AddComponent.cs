using System;

namespace Hyperion.Editor.UI;

public class AddComponent : UIEventHandler
{
    public override void OnAdded(Entity entity)
    {
        base.OnAdded(entity);
    }

    [UIEvent(AllowNested = true)]
    public void LoadComponents()
    {
        Logger.Log(LogLevel.Info, "Load components here");
    }

    [UIEvent(AllowNested = true)]
    public void AddComponentClicked()
    {
        Logger.Log(LogLevel.Info, "AddComponentClicked");
    }

    [UIEvent(AllowNested = true)]
    public void CancelClicked()
    {
        Logger.Log(LogLevel.Info, "CancelClicked");
    }
}