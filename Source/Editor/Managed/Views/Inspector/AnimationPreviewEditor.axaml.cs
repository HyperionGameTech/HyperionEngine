using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AnimationPreviewEditor : UserControl
    {
        private AnimationPreviewViewModel? _shownViewModel;

        public AnimationPreviewEditor()
        {
            InitializeComponent();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);

            _shownViewModel = (AnimationPreviewViewModel)DataContext!;
            _shownViewModel.SetShown(true);
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnDetachedFromVisualTree(e);

            _shownViewModel!.SetShown(false);
            _shownViewModel = null;
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
