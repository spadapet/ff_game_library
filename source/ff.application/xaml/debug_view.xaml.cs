using System.Windows.Controls;
using WpfTools;

namespace ff
{
    public class debug_view_model : PropertyNotifier
    {
    }

    public partial class debug_view : UserControl
    {
        public debug_view_model view_model { get; } = new debug_view_model();

        public debug_view()
        {
            this.InitializeComponent();
        }
    }
}
