using System.Windows.Controls;

namespace editor
{
    public partial class main_ui : UserControl
    {
        public main_vm view_model { get; } = new main_vm();

        public main_ui()
        {
            this.InitializeComponent();
        }
    }
}
