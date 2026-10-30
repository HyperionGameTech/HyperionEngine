using System;

namespace Hyperion
{
    [ClassBinding(Name = "CsgBrushShape")]
    public enum CsgBrushShape : byte
    {
        Box = 0,
        Sphere,
        Cylinder
    }

    [ClassBinding(Name = "CsgOperation")]
    public enum CsgOperation : byte
    {
        Union = 0,
        Subtract,
        Intersect
    }

    [ClassBinding(Name = "CsgEditorExtension")]
    public class CsgEditorExtension : EditorExtension
    {
    }

    [ClassBinding(Name = "EditorCsgState")]
    public class EditorCsgState : ObjectBase
    {
    }

    public static class CsgEditorSubsystemExtensions
    {
        public static CsgEditorExtension? GetCsgEditorExtension(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetExtensionOfClass(new Name(nameof(CsgEditorExtension))) as CsgEditorExtension;
        }

        public static EditorCsgState? GetCsgState(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetCsgEditorExtension()?.GetCsgState();
        }
    }
}
