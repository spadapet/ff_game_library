using System.Windows;
using System.Windows.Input;
using WpfTools;

namespace editor
{
    public class main_vm : PropertyNotifier
    {
        public ICommand file_exit_command => new DelegateCommand(() =>
        {
            Application.Current.MainWindow.Close();
        });
    }
}
