using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class SkeletonChoicePropertyEditor : UserControl
    {
        public SkeletonChoicePropertyEditor()
        {
            InitializeComponent();

            if (this.FindControl<ComboBox>("ChoiceCombo") is ComboBox combo)
            {
                combo.PointerWheelChanged += (_, e) => e.Handled = true;
            }
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
