using System;

namespace Hyperion.Editor.ViewModels
{
    public abstract class EditorPanelViewModel : ViewModelBase
    {
        public string Title { get; }

        public Action? OnClosed { get; protected set; }

        protected EditorPanelViewModel(string title, Action? onClosed = null)
        {
            Title = title ?? throw new ArgumentNullException(nameof(title));
            OnClosed = onClosed;
        }
    }
}
