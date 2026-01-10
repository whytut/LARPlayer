#include <gtk/gtk.h>
#include <string>

std::string show_dialog_keyboard(bool &accepted) {
    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "L:D_N:dialog_PC:T_ID:tetris.username");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 560, 720);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 30);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget* main_vbox = gtk_vbox_new(FALSE, 15);
    gtk_container_add(GTK_CONTAINER(content_area), main_vbox);
    
    GtkWidget* title_label = gtk_label_new("<b><big>New High Score!</big></b>");
    gtk_label_set_use_markup(GTK_LABEL(title_label), TRUE);
    gtk_box_pack_start(GTK_BOX(main_vbox), title_label, FALSE, FALSE, 5);
    
    GtkWidget* separator1 = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), separator1, FALSE, FALSE, 0);
    
    std::string score_text = "Your score: " + std::to_string(game_.score());
    GtkWidget* score_label = gtk_label_new(score_text.c_str());
    gtk_misc_set_alignment(GTK_MISC(score_label), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(main_vbox), score_label, FALSE, FALSE, 10);
    
    GtkWidget* prompt_label = gtk_label_new("Enter your name (up to 10 letters):");
    gtk_box_pack_start(GTK_BOX(main_vbox), prompt_label, FALSE, FALSE, 5);
    
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 10);
    gtk_entry_set_editable(GTK_ENTRY(entry), FALSE);
    gtk_box_pack_start(GTK_BOX(main_vbox), entry, FALSE, FALSE, 5);
    
    // Virtual keyboard
    GtkWidget* keyboard_vbox = gtk_vbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), keyboard_vbox, FALSE, FALSE, 5);
    
    const char* rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    
    for (int row = 0; row < 4; row++) {
        GtkWidget* row_hbox = gtk_hbox_new(TRUE, 3);
        gtk_box_pack_start(GTK_BOX(keyboard_vbox), row_hbox, FALSE, FALSE, 0);
        
        const char* letters = rows[row];
        for (int i = 0; letters[i] != '\0'; i++) {
            char letter[2] = {letters[i], '\0'};
            GtkWidget* key_button = gtk_button_new_with_label(letter);
            
            struct KeyData {
                GtkWidget* entry;
                char letter;
            };
            KeyData* key_data = g_new0(KeyData, 1);
            key_data->entry = entry;
            key_data->letter = letters[i];
            
            g_signal_connect(key_button, "clicked",
                           G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
                               KeyData* kd = (KeyData*)user_data;
                               const char* current = gtk_entry_get_text(GTK_ENTRY(kd->entry));
                               std::string text = current ? current : "";
                               if (text.length() < 10) {
                                   text += kd->letter;
                                   gtk_entry_set_text(GTK_ENTRY(kd->entry), text.c_str());
                               }
                           }),
                           key_data);
            
            g_signal_connect(key_button, "destroy",
                           G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
                               g_free(user_data);
                           }),
                           key_data);
            
            gtk_box_pack_start(GTK_BOX(row_hbox), key_button, TRUE, TRUE, 0);
        }
    }
    
    // Space, Backspace, and Clear button row
    GtkWidget* special_row = gtk_hbox_new(TRUE, 3);
    gtk_box_pack_start(GTK_BOX(keyboard_vbox), special_row, FALSE, FALSE, 0);
    
    GtkWidget* space_button = gtk_button_new_with_label("Space");
    g_signal_connect(space_button, "clicked",
                   G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
                       GtkWidget* entry = (GtkWidget*)user_data;
                       const char* current = gtk_entry_get_text(GTK_ENTRY(entry));
                       std::string text = current ? current : "";
                       if (text.length() < 10) {
                           text += ' ';
                           gtk_entry_set_text(GTK_ENTRY(entry), text.c_str());
                       }
                   }),
                   entry);
    gtk_box_pack_start(GTK_BOX(special_row), space_button, TRUE, TRUE, 0);
    
    GtkWidget* backspace_button = gtk_button_new_with_label("Backspace");
    g_signal_connect(backspace_button, "clicked",
                   G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
                       GtkWidget* entry = (GtkWidget*)user_data;
                       const char* current = gtk_entry_get_text(GTK_ENTRY(entry));
                       std::string text = current ? current : "";
                       if (!text.empty()) {
                           text.pop_back();
                           gtk_entry_set_text(GTK_ENTRY(entry), text.c_str());
                       }
                   }),
                   entry);
    gtk_box_pack_start(GTK_BOX(special_row), backspace_button, TRUE, TRUE, 0);
    
    GtkWidget* clear_button = gtk_button_new_with_label("Clear");
    g_signal_connect(clear_button, "clicked",
                   G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
                       GtkWidget* entry = (GtkWidget*)user_data;
                       gtk_entry_set_text(GTK_ENTRY(entry), "");
                   }),
                   entry);
    gtk_box_pack_start(GTK_BOX(special_row), clear_button, TRUE, TRUE, 0);
    
    GtkWidget* separator2 = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), separator2, FALSE, FALSE, 10);
    
    GtkWidget* button_align = gtk_alignment_new(0.5, 0.5, 1.0, 0.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(button_align), 10, 10, 20, 20);
    gtk_box_pack_start(GTK_BOX(main_vbox), button_align, FALSE, FALSE, 0);
    
    GtkWidget* button_box = gtk_hbox_new(TRUE, 10);
    gtk_container_add(GTK_CONTAINER(button_align), button_box);
    
    GtkWidget* ok_button = gtk_button_new_with_label("OK");
    gtk_box_pack_start(GTK_BOX(button_box), ok_button, TRUE, TRUE, 0);
    
    GtkWidget* cancel_button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(button_box), cancel_button, TRUE, TRUE, 0);
    
    struct DialogData {
        GtkWidget* entry;
        MainWindow* window;
    };
    
    DialogData* data = g_new0(DialogData, 1);
    data->entry = entry;
    data->window = this;
    
    g_signal_connect(ok_button, "clicked",
                     G_CALLBACK(+[](GtkWidget* widget, gpointer user_data) {
                         DialogData* d = (DialogData*)user_data;
                         const char* name = gtk_entry_get_text(GTK_ENTRY(d->entry));
                         std::string username = name ? name : "Player";
                         if (username.empty()) {
                             username = "Player";
                         }
                         
                         for (char& c : username) {
                             if (!std::isalnum(c)) {
                                 c = '_';
                             }
                         }

                         MainWindow* window = d->window;
                         GtkWidget* dialog = gtk_widget_get_toplevel(widget);
                         gtk_widget_destroy(dialog);
                         g_free(d);

                         if (window) {
                             window->add_to_leaderboard(username, window->game_.score());
                             window->show_leaderboard();
                         }
                     }),
                     data);
    
    g_signal_connect(cancel_button, "clicked",
                     G_CALLBACK(+[](GtkWidget* widget, gpointer user_data) {
                         DialogData* d = (DialogData*)user_data;
                         GtkWidget* dialog = gtk_widget_get_toplevel(widget);
                         if (dialog) {
                             gtk_widget_destroy(dialog);
                         }
                         g_free(d);
                     }),
                     data);
    
    gtk_widget_show_all(dialog);
}
