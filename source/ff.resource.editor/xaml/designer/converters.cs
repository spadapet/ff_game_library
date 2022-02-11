using System.Windows;
using WpfTools;

namespace ff
{
    public class bool_to_object_converter : BoolToObjectConverter
    { }

    public sealed class bool_to_visible_converter : BoolToObjectConverter
    {
        public bool_to_visible_converter()
        {
            base.TrueValue = Visibility.Visible;
            base.FalseValue = Visibility.Collapsed;
        }
    }
}
