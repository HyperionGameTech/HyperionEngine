using System;
using System.Runtime.InteropServices;

namespace Hyperion
{

    [HypClassBinding(Name="EditorPropertyPanelBase")]
    public abstract class EditorPropertyPanelBase : UIPanel
    {
        public EditorPropertyPanelBase()
        {
        }

        public abstract void Build(object target);
    }
}