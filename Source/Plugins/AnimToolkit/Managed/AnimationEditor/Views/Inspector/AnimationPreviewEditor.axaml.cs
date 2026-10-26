using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AnimationPreviewEditor : UserControl
    {
        private AnimationPreviewViewModel? _shownViewModel;
        private bool _isAttached;

        public AnimationPreviewEditor()
        {
            InitializeComponent();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);

            _isAttached = true;
            Show(DataContext as AnimationPreviewViewModel);
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnDetachedFromVisualTree(e);

            _isAttached = false;
            Show(null);
        }

        // the template's DataContext binding can arrive after the control is attached, or move it to another item
        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (_isAttached)
            {
                Show(DataContext as AnimationPreviewViewModel);
            }
        }

        private void Show(AnimationPreviewViewModel? viewModel)
        {
            if (viewModel == _shownViewModel)
            {
                return;
            }

            _shownViewModel?.SetShown(false);
            _shownViewModel = viewModel;
            _shownViewModel?.SetShown(true);
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
