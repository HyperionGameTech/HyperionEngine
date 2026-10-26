using System;

namespace Hyperion
{
    [ClassBinding(Name = "EditorImportReview")]
    public abstract class EditorImportReview : ObjectBase
    {
    }

    [ClassBinding(Name = "EditorModelImportReview")]
    public class EditorModelImportReview : EditorImportReview
    {
    }
}
