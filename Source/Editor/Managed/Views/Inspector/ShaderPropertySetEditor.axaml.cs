using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class ShaderPropertySetEditor : UserControl
    {
        public ShaderPropertySetEditor()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
