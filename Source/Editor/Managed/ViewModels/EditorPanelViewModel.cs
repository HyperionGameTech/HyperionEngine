using System;
using Avalonia;

namespace Hyperion.Editor.ViewModels
{
    public abstract class EditorPanelViewModel : ViewModelBase
    {
        public string Title { get; }

        public Action? OnClosed { get; protected set; }

        /// <summary>
        /// The panel's scroll position, persisted for the lifetime of this view model instance so
        /// navigating back to it (via <see cref="Services.PanelService"/>'s stack) restores where the
        /// user left off. A freshly-opened panel starts at the default, Vector.Zero.
        /// </summary>
        private Vector _scrollOffset;
        public Vector ScrollOffset
        {
            get => _scrollOffset;
            set => SetProperty(ref _scrollOffset, value);
        }

        protected EditorPanelViewModel(string title, Action? onClosed = null)
        {
            Title = title ?? throw new ArgumentNullException(nameof(title));
            OnClosed = onClosed;
        }
    }
}
