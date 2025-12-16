using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class EnumFlagsPropertyEditor : UserControl
    {
        public EnumFlagsPropertyEditor()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
