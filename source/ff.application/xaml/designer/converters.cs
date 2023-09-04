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

    public class bool_to_collapsed_converter : ValueConverter
    {
        public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value is bool collapsed && collapsed ? Visibility.Collapsed : Visibility.Visible;
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

    public class bool_to_inverse_converter : ValueConverter
    {
        public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value is not bool bool_value || !bool_value;
        }
    }

    public class level_to_indent_converter : ValueConverter
    {
        public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            double? indent = parameter as double?;
            return value is int level ? new Thickness(level * (indent ?? 8.0), 0, 0, 0) : new Thickness();
        }
    }
}
