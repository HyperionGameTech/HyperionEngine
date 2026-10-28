using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views
{
    public partial class AnimationGraphInspectorView : UserControl
    {
        public AnimationGraphInspectorView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
