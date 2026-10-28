using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AnimationGraphParameterChoiceEditor : UserControl
    {
        public AnimationGraphParameterChoiceEditor()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
