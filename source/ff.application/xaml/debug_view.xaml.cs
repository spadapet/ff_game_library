using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
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

    public class debug_view_model : PropertyNotifier
    {
        public double game_seconds => 1.0;
        public double delta_seconds => 0.125;
        public int frames_per_second => 60;
        public int frame_count => 32;
        public bool debug_visible { get => true; set { } }
        public bool timers_visible { get => true; set { } }
        public bool chart_visible { get => true; set { } }
        public bool stopped_visible { get => true; set { } }
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
