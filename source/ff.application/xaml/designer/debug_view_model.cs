using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using System.Windows.Input;
using System.Windows.Media;
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
    }

    public class debug_timer_model : PropertyNotifier
    {
        public string name { get; } = $"Timer {Random.Shared.Next(1000)}";
        public Brush name_brush => Brushes.White;
        public double time_ms => 10.0;
        public int level { get; } = Random.Shared.Next(4);
        public int hit_total => 100;
        public int hit_last_frame => 10;
        public int hit_per_second => 60;
    }

    public class debug_view_model : PropertyNotifier
    {
        public debug_view_model()
        {
            this.selected_page_ = this.pages[0];
        }

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

        private bool timers_updating_ = true;
        public bool timers_updating
        {
            get => this.timers_updating_;
            set => this.SetProperty(ref this.timers_updating_, value);
        }

        private int timer_update_speed_ = 16;
        public int timer_update_speed
        {
            get => this.timer_update_speed_;
            set => this.SetProperty(ref this.timer_update_speed_, value);
        }

        private bool chart_visible_;
        public bool chart_visible
        {
            get => this.chart_visible_;
            set => this.SetProperty(ref this.chart_visible_, value);
        }

        private bool building_resources_;
        public bool building_resources
        {
            get => this.building_resources_;
            set
            {
                if (this.SetProperty(ref this.building_resources_, value))
                {
                    this.build_resources_command_?.UpdateCanExecute();
                }
            }
        }

        public ObservableCollection<debug_page_model> pages { get; } = new()
        {
            new debug_page_model(is_none: true),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
            new debug_page_model(),
        };

        public ObservableCollection<debug_timer_model> timers{ get; } = new()
        {
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
            new debug_timer_model(),
        };

        private debug_page_model selected_page_;
        public debug_page_model selected_page
        {
            get => this.selected_page_;
            set => this.SetProperty(ref this.selected_page_, value ?? this.pages[0]);
        }

        private debug_page_model selected_timer_;
        public debug_page_model selected_timer
        {
            get => this.selected_timer_;
            set => this.SetProperty(ref this.selected_timer_, value);
        }

        public ICommand close_command => new DelegateCommand(() =>
        {
            App.Current.MainWindow.Close();
        });

        private DelegateCommand build_resources_command_;
        public ICommand build_resources_command => this.build_resources_command_ ??= new DelegateCommand(async () =>
        {
            this.building_resources = true;
            await Task.Delay(4000);
            this.building_resources = false;
        }, () => !this.building_resources);

        public ICommand select_page_command => new DelegateCommand(param =>
        {
            if (param is debug_page_model page)
            {
                this.selected_page = page;
            }
        });
    }
}
