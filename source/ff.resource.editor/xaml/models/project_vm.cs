using WpfTools;

namespace editor
{
    public class project_vm : PropertyNotifier
    {
        public bool dirty => true;
        public string file_path => "c:\\fake\\project_file.txt";
        public string file_name => "project_file.txt";
    }
}
