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
        Logger.Log(LogType.Info, "Load components here");
    }

    [UIEvent(AllowNested = true)]
    public void AddComponentClicked()
    {
        Logger.Log(LogType.Info, "AddComponentClicked");
    }

    [UIEvent(AllowNested = true)]
    public void CancelClicked()
    {
        Logger.Log(LogType.Info, "CancelClicked");
    }
}