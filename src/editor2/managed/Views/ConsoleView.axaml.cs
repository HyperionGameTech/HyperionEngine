using System;
using System.Collections.Specialized;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor
{
    public partial class ConsoleView : UserControl
    {
        private ListBox _listBox;
        private TextBox _commandTextBox;
        private ScrollViewer _scrollViewer;
        private bool _autoScroll = true;
        private ConsoleViewModel _viewModel;

        public ConsoleView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
            _listBox = this.FindControl<ListBox>("LogListBox");
            _commandTextBox = this.FindControl<TextBox>("CommandTextBox");
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            UpdateViewModelSubscription();
            
            // Defer ScrollViewer lookup to ensure template is applied
            Dispatcher.UIThread.Post(() =>
            {
                _scrollViewer = _listBox.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
                if (_scrollViewer != null)
                {
                    _scrollViewer.ScrollChanged += OnScrollChanged;
                    
                    // Initial scroll if needed
                    if (_autoScroll)
                    {
                        ScrollToBottom();
                    }
                }

                _commandTextBox?.Focus();
            }, DispatcherPriority.Loaded);
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnDetachedFromVisualTree(e);
            
            if (_viewModel != null)
            {
                // why is it protected?
                ((INotifyCollectionChanged)_viewModel.Logs).CollectionChanged -= OnLogsCollectionChanged;
                _viewModel = null;
            }

            if (_scrollViewer != null)
            {
                _scrollViewer.ScrollChanged -= OnScrollChanged;
                _scrollViewer = null;
            }
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);
            UpdateViewModelSubscription();
        }

        private void UpdateViewModelSubscription()
        {
            if (DataContext is ConsoleViewModel vm && vm != _viewModel)
            {
                if (_viewModel != null)
                {
                    ((INotifyCollectionChanged)_viewModel.Logs).CollectionChanged -= OnLogsCollectionChanged;

                }
                
                _viewModel = vm;
                ((INotifyCollectionChanged)_viewModel.Logs).CollectionChanged += OnLogsCollectionChanged;
            }
        }

        private void OnScrollChanged(object sender, ScrollChangedEventArgs e)
        {
            if (_scrollViewer == null) return;

            // Check if we are near the bottom
            bool isAtBottom = _scrollViewer.Offset.Y >= (_scrollViewer.Extent.Height - _scrollViewer.Viewport.Height - 5);

            if (isAtBottom)
            {
                _autoScroll = true;
            }
            else
            {
                // Only disable auto-scroll if we have scrollable content
                if (_scrollViewer.Extent.Height > _scrollViewer.Viewport.Height)
                {
                    _autoScroll = false;
                }
            }
        }

        private void OnLogsCollectionChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            if (_autoScroll)
            {
                // Post to UI thread to allow layout to update before scrolling
                Dispatcher.UIThread.Post(ScrollToBottom, DispatcherPriority.Background);
            }
        }

        private void ScrollToBottom()
        {
            if (_scrollViewer != null)
            {
                _scrollViewer.ScrollToEnd();
            }
            else if (_listBox.ItemCount > 0)
            {
                _listBox.ScrollIntoView(_listBox.ItemCount - 1);
            }
        }
    }
}
