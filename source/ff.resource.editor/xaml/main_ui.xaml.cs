using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using WpfTools;

namespace editor
{
    public class main_view_model : PropertyNotifier
    {
        public ICommand file_exit_command => new DelegateCommand(() =>
        {
            Application.Current.MainWindow.Close();
        });
    }

    public partial class main_ui : UserControl
    {
        public main_view_model view_model { get; } = new main_view_model();

        public main_ui()
        {
            this.InitializeComponent();
        }
    }
}
