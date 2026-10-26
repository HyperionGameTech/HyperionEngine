using System;

namespace Hyperion
{
    [ClassBinding(Name = "EditorIsolatedEditState")]
    public abstract class EditorIsolatedEditState : ObjectBase
    {
    }

    [ClassBinding(Name = "EditorPrefabEditState")]
    public class EditorPrefabEditState : EditorIsolatedEditState
    {
    }
}
