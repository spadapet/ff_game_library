using System.Windows;
using System.Windows.Input;
using WpfTools;

namespace editor
{
    public class main_vm : PropertyNotifier
    {
        public ICommand file_new_command => new DelegateCommand(() => { });
        public ICommand file_open_command => new DelegateCommand(() => { });
        public ICommand file_save_command => new DelegateCommand(() => { });
        public ICommand file_save_as_command => new DelegateCommand(() => { });

        public ICommand file_exit_command => new DelegateCommand(() =>
        {
            Application.Current.MainWindow.Close();
        });

        public ICommand close_dialog_command => new DelegateCommand(() =>
        {
            this.modal_dialog = null;
        });

        public bool has_modal_dialog => this.modal_dialog_ != null;

        private dialog_content_base modal_dialog_ = new save_project_dialog();
        public dialog_content_base modal_dialog
        {
            get => this.modal_dialog_;
            set
            {
                if (this.SetProperty(ref this.modal_dialog_, value))
                {
                    this.OnPropertyChanged(nameof(this.has_modal_dialog));
                }
            }
        }
    }
}
