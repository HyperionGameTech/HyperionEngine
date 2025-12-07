using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using System.Collections.Specialized;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor
{
    public partial class ConsoleView : UserControl
    {
        private ListBox _listBox;

        public ConsoleView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);

            _listBox = this.FindControl<ListBox>("LogListBox");
        }
        
        protected override void OnDataContextChanged(System.EventArgs e)
        {
            base.OnDataContextChanged(e);
            
            if (DataContext is ConsoleViewModel vm)
            {
                vm.Logs.CollectionChanged += Logs_CollectionChanged;
            }
        }

        private void Logs_CollectionChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            if (e.Action == NotifyCollectionChangedAction.Add && _listBox != null && _listBox.ItemCount > 0)
            {
                _listBox.ScrollIntoView(_listBox.ItemCount - 1);
            }
        }
    }
}
