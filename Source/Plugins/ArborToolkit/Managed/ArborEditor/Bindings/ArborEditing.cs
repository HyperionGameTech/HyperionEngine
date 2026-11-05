using System;

namespace Hyperion
{
    public enum ArborPlantKind : uint
    {
        Tree = 0,
        Cover,
        Rock
    }

    public enum ArborControlKind : uint
    {
        Ranged = 0,
        Count,
        Choice,
        Toggle
    }

    [ClassBinding(Name = "ArborEditorExtension")]
    public class ArborEditorExtension : EditorExtension
    {
    }

    [ClassBinding(Name = "EditorArborState")]
    public class EditorArborState : EditorIsolatedEditState
    {
    }

    public static class ArborEditorSubsystemExtensions
    {
        public static ArborEditorExtension? GetArborEditorExtension(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetExtensionOfClass(new Name(nameof(ArborEditorExtension))) as ArborEditorExtension;
        }

        public static EditorArborState? GetArborState(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetArborEditorExtension()?.GetArborState();
        }
    }
}
