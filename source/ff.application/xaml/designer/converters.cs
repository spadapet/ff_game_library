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
}
