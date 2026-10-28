namespace Hyperion.Editor.ViewModels
{
    public class AnimationGraphInspectorViewModel : ViewModelBase
    {
        public AnimationGraphInspectorViewModel(AnimationGraphDocumentViewModel document)
        {
            Document = document;
        }

        public AnimationGraphDocumentViewModel Document { get; }

        public void NotifyChanged() => OnPropertyChanged(nameof(Document));
    }
}
