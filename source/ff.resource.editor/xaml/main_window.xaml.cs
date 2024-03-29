﻿using System.Windows;

namespace editor
{
    public partial class main_window : Window
    {
        public main_vm view_model { get; } = new main_vm();

        public main_window()
        {
            this.InitializeComponent();
        }

        private void on_request_close_dialog(object sender, RoutedEventArgs args)
        {
            this.view_model.modal_dialog = null;
        }
    }
}
