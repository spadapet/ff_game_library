using System.Windows;
using WpfTools;

namespace ff
{
    public sealed class bool_to_visible_converter : BoolToObjectConverter
    {
        public bool_to_visible_converter()
        {
            base.TrueValue = Visibility.Visible;
            base.FalseValue = Visibility.Collapsed;
        }
    }

    public sealed class bool_to_collapsed_converter : BoolToObjectConverter
    {
        public bool_to_collapsed_converter()
        {
            base.TrueValue = Visibility.Collapsed;
            base.FalseValue = Visibility.Visible;
        }
    }

    public class bool_to_object_converter : BoolToObjectConverter
    { }

    public sealed class object_to_visible_converter : ObjectToObjectConverter
    {
        public object_to_visible_converter()
        {
            base.NonNullValue = Visibility.Visible;
            base.NullValue = Visibility.Collapsed;
        }
    }

    public sealed class object_to_collapsed_converter : ObjectToObjectConverter
    {
        public object_to_collapsed_converter()
        {
            base.NonNullValue = Visibility.Collapsed;
            base.NullValue = Visibility.Visible;
        }
    }

    public class object_to_object_converter : ObjectToObjectConverter
    { }
}
