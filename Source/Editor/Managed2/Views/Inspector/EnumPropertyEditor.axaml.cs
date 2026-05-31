using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class EnumPropertyEditor : UserControl
    {
        public EnumPropertyEditor()
        {
            InitializeComponent();
            // Prevent mouse wheel from changing the ComboBox selection
            var combo = this.FindControl<ComboBox>("EnumCombo");
            if (combo != null)
            {
                combo.PointerWheelChanged += Combo_PointerWheelChanged;
            }
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void Combo_PointerWheelChanged(object? sender, PointerWheelEventArgs e)
        {
            // Mark event handled so the parent ScrollViewer won't let the ComboBox change selection via wheel
            e.Handled = true;
        }
    }
}
