using System;
using System.Globalization;
using System.Windows;
using WpfTools;

namespace ff
{
    public class bool_to_visible_converter : ValueConverter
    {
        public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value is bool visible && visible ? Visibility.Visible : Visibility.Collapsed;
        }
    }

    public class bool_to_object_converter : ValueConverter
    {
        public object TrueValue { get; set; }
        public object FalseValue { get; set; }

        public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value is bool visible && visible ? this.TrueValue : this.FalseValue;
        }
    }
}
