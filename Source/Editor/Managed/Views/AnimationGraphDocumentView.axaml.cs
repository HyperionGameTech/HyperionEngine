using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views
{
    public partial class AnimationGraphDocumentView : UserControl
    {
        public AnimationGraphDocumentView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
