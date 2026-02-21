
using System;
using System.IO;
using Hyperion;


namespace Hyperion.Editor.UI;

public class ContentBrowser : UIEventHandler
{
    public override void OnAdded(Entity entity)
    {
        base.OnAdded(entity);
    }

    [UIEvent(AllowNested = true)]
    public void ImportClicked()
    {
        Logger.Log(LogLevel.Info, "Import content clicked");
    }

    // temp debug
    public override void Update(float deltaTime)
    {
        // Do nothing
        Console.WriteLine("ContentBrowser Update called with deltaTime: " + deltaTime);
    }
}