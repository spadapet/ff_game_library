using System.Windows.Controls;

namespace ff
{
    public partial class stopped_view : UserControl
    {
        public debug_view_model view_model { get; } = new debug_view_model();

        public stopped_view()
        {
            this.InitializeComponent();
        }
    }
}
