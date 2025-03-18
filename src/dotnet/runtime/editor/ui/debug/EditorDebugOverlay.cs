using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EditorDebugOverlayBase")]
    public abstract class EditorDebugOverlayBase : HypObject
    {
        public EditorDebugOverlayBase()
        {
        }

        public abstract void Update(float delta);
        public abstract UIObject CreateUIObject(UIObject spawnParent);
        public abstract Name GetName();
        public abstract bool IsEnabled();
    }
}