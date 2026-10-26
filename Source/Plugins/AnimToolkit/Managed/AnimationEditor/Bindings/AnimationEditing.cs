using System;

namespace Hyperion
{
    [ClassBinding(Name = "AnimationEditorExtension")]
    public class AnimationEditorExtension : EditorExtension
    {
    }

    [ClassBinding(Name = "EditorAnimationEditState")]
    public class EditorAnimationEditState : EditorIsolatedEditState
    {
    }

    [ClassBinding(Name = "EditorBoneNode")]
    public class EditorBoneNode : Node
    {
    }

    [ClassBinding(Name = "EditorSocketNode")]
    public class EditorSocketNode : Node
    {
    }

    public static class AnimationEditorSubsystemExtensions
    {
        public static AnimationEditorExtension? GetAnimationEditorExtension(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetExtensionOfClass(new Name(nameof(AnimationEditorExtension))) as AnimationEditorExtension;
        }

        public static EditorAnimationEditState? GetAnimationEditState(this EditorSubsystem editorSubsystem)
        {
            return editorSubsystem.GetAnimationEditorExtension()?.GetEditState();
        }
    }
}
