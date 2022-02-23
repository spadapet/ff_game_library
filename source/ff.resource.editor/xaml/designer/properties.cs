using System.Windows;

namespace editor
{
    public static class properties
    {
        public static readonly DependencyProperty ModalFlashProperty = DependencyProperty.RegisterAttached("ModalFlash", typeof(bool), typeof(properties),
            new FrameworkPropertyMetadata(false, FrameworkPropertyMetadataOptions.Inherits));

        public static bool GetModalFlash(DependencyObject obj)
        {
            return (bool)obj.GetValue(properties.ModalFlashProperty);
        }

        public static void SetModalFlash(DependencyObject obj, bool value)
        {
            obj.SetValue(properties.ModalFlashProperty, value);
        }
    }
}
