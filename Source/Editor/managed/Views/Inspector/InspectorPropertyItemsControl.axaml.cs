using System.Collections;
using Avalonia;
using Avalonia.Controls;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class InspectorPropertyItemsControl : UserControl
    {
        public static readonly StyledProperty<IEnumerable?> ItemsSourceProperty =
            AvaloniaProperty.Register<InspectorPropertyItemsControl, IEnumerable?>(nameof(ItemsSource));

        public IEnumerable? ItemsSource
        {
            get => GetValue(ItemsSourceProperty);
            set => SetValue(ItemsSourceProperty, value);
        }

        public InspectorPropertyItemsControl()
        {
            InitializeComponent();
        }
    }
}
