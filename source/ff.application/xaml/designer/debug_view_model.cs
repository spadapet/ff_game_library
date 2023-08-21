using WpfTools;

namespace ff
{
    public class debug_view_model : PropertyNotifier
    {
        public double game_seconds => 90000.0;
        public double delta_seconds => 0.125;
        public int frames_per_second => 60;
        public int frame_count => 5400000;
        public bool debug_visible { get => true; set { } }
        public bool stopped_visible { get => true; set { } }

        private bool extensions_visible_;
        public bool extensions_visible
        {
            get => this.extensions_visible_;
            set => this.SetProperty(ref this.extensions_visible_, value);
        }

        private bool timers_visible_;
        public bool timers_visible
        {
            get => this.timers_visible_;
            set => this.SetProperty(ref this.timers_visible_, value);
        }
    }
}
