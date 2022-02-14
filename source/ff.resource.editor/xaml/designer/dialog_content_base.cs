using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using WpfTools;

namespace editor
{
    public class dialog_content_base : UserControl
    {
        public static readonly DependencyProperty TitleProperty = DependencyProperty.Register("Title", typeof(string), typeof(dialog_content_base));
        public string Title
        {
            get => (string)GetValue(TitleProperty);
            set => SetValue(TitleProperty, value);
        }

        public static readonly DependencyProperty FooterProperty = DependencyProperty.Register("Footer", typeof(UIElement), typeof(dialog_content_base));
        public UIElement Footer
        {
            get => (UIElement)GetValue(FooterProperty);
            set => SetValue(FooterProperty, value);
        }

        public static readonly RoutedEvent RequestCloseEvent = EventManager.RegisterRoutedEvent("RequestClose", RoutingStrategy.Bubble, typeof(RoutedEventHandler), typeof(dialog_content_base));

        public ICommand ok_command => new DelegateCommand(() =>
        {
            this.OnRequestClose();
        });

        public ICommand cancel_command => new DelegateCommand(() =>
        {
            this.OnRequestClose();
        });

        public event RoutedEventHandler RequestClose
        {
            add => this.AddHandler(dialog_content_base.RequestCloseEvent, value);
            remove => this.RemoveHandler(dialog_content_base.RequestCloseEvent, value);
        }

        public virtual void OnRequestClose()
        {
            this.RaiseEvent(new RoutedEventArgs(dialog_content_base.RequestCloseEvent, this));
        }
    }
}
