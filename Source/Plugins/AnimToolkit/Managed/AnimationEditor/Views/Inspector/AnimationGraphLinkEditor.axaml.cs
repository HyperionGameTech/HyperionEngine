using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AnimationGraphLinkEditor : UserControl
    {
        public AnimationGraphLinkEditor()
        {
            InitializeComponent();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);

            (DataContext as AnimationGraphLinkViewModel)?.RefreshValue();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
