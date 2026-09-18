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

        /// <summary>
        /// Show property descriptions as text under each row, instead of only as label tooltips.
        /// Inherited, so nested struct / sub-object property lists follow the outermost setting.
        /// </summary>
        public static readonly StyledProperty<bool> ShowDescriptionsProperty =
            AvaloniaProperty.Register<InspectorPropertyItemsControl, bool>(nameof(ShowDescriptions), defaultValue: false, inherits: true);

        public bool ShowDescriptions
        {
            get => GetValue(ShowDescriptionsProperty);
            set => SetValue(ShowDescriptionsProperty, value);
        }

        public InspectorPropertyItemsControl()
        {
            InitializeComponent();
        }
    }
}
