using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorManipulationMode")]
    public enum EditorManipulationMode
    {
        None = 0,
        Translate = 1,
        Rotate = 2,
        Scale = 3
    }

    [ClassBinding(Name = "EditorGizmoBase")]
    public abstract class EditorGizmoBase : ObjectBase
    {
        public EditorGizmoBase()
        {
        }

        public EditorManipulationMode ManipulationMode => this.GetManipulationMode();
        public string MenuText => this.GetMenuText();
        public bool Dragging => this.IsDragging();
        public Node Node => this.GetNode();
        public int Priority => this.GetPriority();
    }

    [ClassBinding(Name = "NullEditorGizmo")]
    public class NullEditorGizmo : EditorGizmoBase
    {
        public NullEditorGizmo()
        {
        }
    }

    [ClassBinding(Name = "TranslateEditorGizmo")]
    public class TranslateEditorGizmo : EditorGizmoBase
    {
        public TranslateEditorGizmo()
        {
        }
    }

    [ClassBinding(Name = "RotateEditorGizmo")]
    public class RotateEditorGizmo : EditorGizmoBase
    {
        public RotateEditorGizmo()
        {
        }
    }

    [ClassBinding(Name = "ScaleEditorGizmo")]
    public class ScaleEditorGizmo : EditorGizmoBase
    {
        public ScaleEditorGizmo()
        {
        }
    }

    [ClassBinding(Name = "VolumeEditorGizmo")]
    public class VolumeEditorGizmo : EditorGizmoBase
    {
        public VolumeEditorGizmo()
        {
        }
    }
}
