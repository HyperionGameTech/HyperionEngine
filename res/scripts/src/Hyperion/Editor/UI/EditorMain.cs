
using System;
using System.IO;
using System.Linq;
using System.Text;
using Hyperion;
using Hyperion.Editor.UI.Overlays;

namespace Hyperion.Editor.UI;

public class CustomSystem : ScriptableSystem
{
    public CustomSystem()
    {
        Logger.Log(LogType.Info, "CustomSystem constructor called");
    }

    protected override ComponentInfo[] GetComponentInfos()
    {
        return new ComponentInfo[]
        {
            // new ComponentInfo(Class.GetClass<LightComponent>().TypeId, ComponentRWFlags.Read, true)
        };
    }

    public override bool AllowUpdate()
    {
        return false;
    }

    public override void OnEntityAdded(Entity entity)
    {
        Logger.Log(LogType.Info, "CustomSystem OnEntityAdded called for entity: " + entity.Id);
    }

    public override void Init()
    {
        base.Init();
        Logger.Log(LogType.Info, "CustomSystem Init called");
    }

    public override void Process(float delta)
    {
    }
}

public class TestEditorTask : LongRunningEditorTask
{
    public TestEditorTask()
    {
    }

    public override void Cancel()
    {
        Logger.Log(LogType.Info, "Cancel task");
    }

    public override bool IsCompleted()
    {
        return false;
    }

    public override void Process()
    {
        Logger.Log(LogType.Info, "Process task! testing");
    }
}