using editor;
using System.Collections.ObjectModel;
using System.IO;
using WpfTools;

namespace editor
{
    public abstract class file_vm : PropertyNotifier
    {
        public abstract string full_path { get; }
        public string file_name => Path.GetFileName(this.full_path);
    }

    public class plugin_vm : file_vm
    {
        public override string full_path => @"c:\fake\plugin_file.dll";
    }

    public class source_vm : file_vm
    {
        public override string full_path => @"c:\fake\source_file.res.json";
    }

    public class project_vm : PropertyNotifier
    {
        public bool dirty => true;
        public string file_path => @"c:\fake\project_file.txt";
        public string file_name => "project_file.txt";

        public ObservableCollection<plugin_vm> plugins { get; } = new()
        {
            new plugin_vm(),
            new plugin_vm(),
            new plugin_vm(),
        };

        public ObservableCollection<source_vm> sources { get; } = new()
        {
            new source_vm(),
            new source_vm(),
            new source_vm(),
        };
    }
}
