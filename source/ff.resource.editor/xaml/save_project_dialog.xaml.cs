namespace editor
{
    public partial class save_project_dialog : dialog_content_base
    {
        public project_vm project { get; } = new project_vm();

        public save_project_dialog()
        {
            this.InitializeComponent();
        }
    }
}
