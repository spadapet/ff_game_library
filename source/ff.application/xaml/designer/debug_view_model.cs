using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using WpfTools;

namespace ff
{
    public class debug_page_model : PropertyNotifier
    {
        public const string none_name = "None";

        public debug_page_model(bool is_none = false)
        {
            this.name = is_none
                ? debug_page_model.none_name
                : $"Debug Page {Random.Shared.Next(1000)}";
        }

        public string name { get; }
        public bool is_none => this.name == debug_page_model.none_name;
        public bool removed => false;
    }

    public class debug_view_model : PropertyNotifier
    {
        public double advance_seconds => 90000.0;
        public int frames_per_second => 60;
        public int frame_count => 5400000;
        public bool debug_visible { get => true; set { } }
        public bool stopped_visible { get => true; set { } }
        public bool has_pages => this.pages.Count > 0;
        public bool page_visible => false;

        private bool page_picker_visible_;
        public bool page_picker_visible
        {
            get => this.page_picker_visible_;
            set => this.SetProperty(ref this.page_picker_visible_, value);
        }

        private bool timers_visible_;
        public bool timers_visible
        {
            get => this.timers_visible_;
            set => this.SetProperty(ref this.timers_visible_, value);
        }

        private bool chart_visible_;
        public bool chart_visible
        {
            get => this.chart_visible_;
            set => this.SetProperty(ref this.chart_visible_, value);
        }

        public ObservableCollection<debug_page_model> pages { get; } = new()
        {
            new debug_page_model(is_none: true),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
        };

        private debug_page_model selected_page_;
        public debug_page_model selected_page
        {
            get => this.selected_page_;
            set => this.SetProperty(ref this.selected_page_, value);
        }

        public ICommand close_command { get; } = new DelegateCommand(() =>
        {
            App.Current.MainWindow.Close();
        });
    }
}
