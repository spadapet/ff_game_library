using System.Windows.Controls;
using WpfTools;

namespace ff
{
    public class debug_view_model : PropertyNotifier
    {
        public double ups => 60.0;
        public double rps => 60.0;
        public double fps => 60.0;
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
