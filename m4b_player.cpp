#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string>
#include <fstream>

#include <map>

#include "music_backend.h"
#include "database_manager.h"
#include "openlipc/openlipc.h"

// Assets
#include "assets/play_pause_icon.h"
#include "assets/fast_forward_icon.h"
#include "assets/fast_rewind_icon.h"
#include "assets/title.h"
#include "assets/bluetooth_icon.h"
#include "assets/display_icon.h"
#include "assets/sunny_icon.h"
#include "assets/close_icon.h"
#include "assets/open_icon.h"
#include "assets/history_icon.h"
#include "assets/bookmarklist_icon.h"
#include "assets/bookmark_icon.h"
#include "assets/chapters_icon.h"

#define DESKTOP_W_SIZE 600
#define DESKTOP_H_SIZE 800

MusicBackend backend;
DatabaseManager db;
GtkWidget *window;
GtkWidget *progress_bar;
GtkWidget *time_label;
GtkWidget *cover_image;
GtkWidget *title_label;
GtkWidget *artist_label;
GtkWidget *album_label;
GtkWidget *play_pause_btn;

bool user_is_seeking = false;
std::string current_file;
int last_timestamp = 0;
int flIntensity = 0;
bool dispUpdate=true;

static LIPC * lipcInstance = 0;

void openLipcInstance() {
	if (lipcInstance == 0) {
		lipcInstance = LipcOpen("com.kbarni.lark");
	}
}

void closeLipcInstance() {
	if (lipcInstance != 0) {
		LipcClose(lipcInstance);
	}
}

void enableSleep() {
    LipcSetIntProperty(lipcInstance,"com.lab126.powerd","preventScreenSaver",0);
}

void disableSleep() {
    LipcSetIntProperty(lipcInstance,"com.lab126.powerd","preventScreenSaver",1);
}

void toggleFrontLight(){
    int intensity = 0;
    LipcGetIntProperty(lipcInstance,"com.lab126.powerd","flIntensity",&intensity);
    if(intensity == 0) {
        LipcSetIntProperty(lipcInstance,"com.lab126.powerd","flIntensity",flIntensity);
    } else {
        flIntensity=intensity;
        LipcSetIntProperty(lipcInstance,"com.lab126.powerd","flIntensity",0);
    }
}

GtkWidget* create_button_from_icon(const guint8* icon_data, int padding) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_inline(-1, icon_data, FALSE, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    
    GtkWidget *align = gtk_alignment_new(0.5, 0.5, 0, 0); 
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), padding, padding, padding, padding);
    gtk_container_add(GTK_CONTAINER(align), image);

    GtkWidget *button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), align);
    
    g_object_unref(pixbuf);
    return button;
}

void set_button_icon(GtkWidget *button, const unsigned char *icon_data) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_inline(-1, icon_data, FALSE, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_button_set_image(GTK_BUTTON(button), image);
    gtk_widget_show(image);
}

std::string get_home_dir() {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return std::string(home ? home : ".");
}

std::string get_db_path() {
    return get_home_dir() + "/.lark_player.db";
}

void save_history() {
    if (!current_file.empty()) {
        gint64 pos = backend.get_position() / GST_SECOND;
        gint64 dur = backend.get_duration() / GST_SECOND;
        // If duration is 0 (e.g. stopped), try to get it from book if we can, or just pass 0.
        // updateBookProgress handles insert/update.
        db.updateBookProgress(current_file, (int)pos, (int)dur, false);
        db.setSetting("last_opened_file", current_file);
    }
}

void init_database() {
    std::string db_path = get_db_path();
    if (!db.init(db_path)) {
        g_print("Failed to initialize database at %s\n", db_path.c_str());
        return;
    }

    // Check for legacy history and migrate
    std::string legacy_path = get_home_dir() + "/.lark_history";
    std::ifstream legacy_file(legacy_path);
    if (legacy_file.good()) {
        legacy_file.close();
        g_print("Migrating legacy history from %s\n", legacy_path.c_str());
        if (db.migrateFromLegacy(legacy_path)) {
            g_print("Migration successful.\n");
        } else {
            g_print("Migration failed.\n");
        }
    }

    // Restore last opened file
    std::string last_file = db.getLastPlayedFile();
    if (!last_file.empty() && current_file.empty()) {
        current_file = last_file;
    }
}

void update_metadata_ui() {
    if (!backend.meta_title.empty()) {
        char *markup = g_markup_printf_escaped("<span font_desc='Sans Bold 24'>%s</span>", backend.meta_title.c_str());
        gtk_label_set_markup(GTK_LABEL(title_label), markup);
        g_free(markup);
    } else {
        gtk_label_set_text(GTK_LABEL(title_label), "");
    }

    if (!backend.meta_artist.empty()) {
        char *markup = g_markup_printf_escaped("<span font_desc='Sans Italic 14'>%s</span>", backend.meta_artist.c_str());
        gtk_label_set_markup(GTK_LABEL(artist_label), markup);
        g_free(markup);
    } else {
        gtk_label_set_text(GTK_LABEL(artist_label), "");
    }

    if (!backend.cover_art.empty()) {
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        gdk_pixbuf_loader_write(loader, backend.cover_art.data(), backend.cover_art.size(), NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) {
            GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 350, 450, GDK_INTERP_BILINEAR);
            gtk_image_set_from_pixbuf(GTK_IMAGE(cover_image), scaled);
            g_object_unref(scaled);
        }
        g_object_unref(loader);
    } else {
        gtk_image_clear(GTK_IMAGE(cover_image));
    }
}

gboolean update_ui(gpointer data) {
    (void)data;
    if (!backend.is_playing && !backend.is_paused) return TRUE;

    gint64 pos = backend.get_position();
    gint64 len = backend.get_duration();

    // Format: 00:01:23 / 02:10:20
    int pos_sec = pos / GST_SECOND;
    int len_sec = len / GST_SECOND;
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d / %02d:%02d:%02d", 
             pos_sec / 3600, (pos_sec % 3600) / 60, pos_sec % 60,
             len_sec / 3600, (len_sec % 3600) / 60, len_sec % 60);
    
    if(dispUpdate)
        gtk_label_set_text(GTK_LABEL(time_label), buf);
    else
        gtk_label_set_text(GTK_LABEL(time_label), "          ");
    return TRUE;
}

void on_play_pause_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    if (backend.is_playing) {
        backend.pause();
    } else {
        if (!current_file.empty()) {
            if (backend.is_paused) {
                backend.pause();
            } else {
                backend.play_file(current_file.c_str(), last_timestamp);
            }
        }
    }
}

void jump_relative(int seconds) {
    if (current_file.empty()) return;
    
    gint64 duration = backend.get_duration() / GST_SECOND;
    
    int current_pos = last_timestamp;
    if (backend.is_playing || backend.is_paused) {
        current_pos = backend.get_position() / GST_SECOND;
    }
    
    int new_pos = current_pos + seconds;
    if (new_pos < 0) new_pos = 0;
    if (duration > 0 && new_pos > duration) new_pos = duration;
    
    last_timestamp = new_pos;
    
    backend.play_file(current_file.c_str(), new_pos);
}

void on_fl_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    toggleFrontLight();
}

void on_bluetooth_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    LipcSetStringProperty(lipcInstance,"com.lab126.btfd","BTenable","1:1");
    LipcSetStringProperty(lipcInstance,"com.lab126.pillow","customDialog","{\"name\":\"bt_wizard_dialog\", \"clientParams\": {\"show\":true, \"winmgrModal\":true, \"replySrc\":\"\"}}");
}

void on_displayUpdate_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    dispUpdate = !(dispUpdate);
}

void on_rewind_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    jump_relative(-30);
}

void on_ff_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    jump_relative(30);
}

void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    LipcSetIntProperty(lipcInstance,"com.lab126.powerd","flIntensity",flIntensity);
    LipcSetIntProperty(lipcInstance,"com.lab126.btfd","ensureBTconnection",0);
    enableSleep();
    closeLipcInstance();
    save_history();
    gtk_main_quit();
}

void on_close_clicked(GtkWidget *widget, gpointer data) {
    on_destroy(widget, data);
}

void on_file_open(const char* filepath) {
    if (!filepath) return;
    
    if (!current_file.empty() && (backend.is_playing || backend.is_paused)) {
        gint64 pos = backend.get_position() / GST_SECOND;
        gint64 dur = backend.get_duration() / GST_SECOND;
        db.updateBookProgress(current_file, (int)pos, (int)dur, false);
    }

    backend.stop();
    
    current_file = filepath;
    db.setSetting("last_opened_file", current_file);
    
    Book book;
    if (db.getBookProgress(filepath, book)) {
        last_timestamp = book.last_position;
    } else {
        last_timestamp = 0;
    }

    g_print("Reading metadata for %s\n", filepath);
    backend.read_metadata(filepath);
    g_print("Metadata read: Title='%s', Artist='%s', Album='%s'\n",
            backend.meta_title.c_str(),
            backend.meta_artist.c_str(),
            backend.meta_album.c_str());
    update_metadata_ui();
    g_print("Starting playback for %s at %d seconds\n", filepath, last_timestamp);
    backend.play_file(filepath, last_timestamp);
}

void on_open_dialog_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("L:A_N:application_PC:TS_ID:com.kbarni.m4bplayer",
                                         GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        last_timestamp = 0;
        on_file_open(filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

void on_history_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("L:A_N:application_PC:TS_ID:com.kbarni.m4bplayer",
                                                     GTK_WINDOW(window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                     NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *tree_view = gtk_tree_view_new();
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    
    std::vector<Book> history = db.getHistory();
    for (const auto& item : history) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, item.filepath.c_str(), 1, item.last_position, -1);
    }
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("File", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    
    gtk_container_add(GTK_CONTAINER(content_area), tree_view);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
        GtkTreeIter iter;
        GtkTreeModel *model;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            char *file;
            gtk_tree_model_get(model, &iter, 0, &file, -1);
            on_file_open(file);
            g_free(file);
        }
    }
    
    gtk_widget_destroy(dialog);
}

void on_add_bookmark_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;
    if (current_file.empty()) return;
    int pos = backend.get_position() / GST_SECOND;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Bookmark %02d:%02d:%02d", 
             pos / 3600, (pos % 3600) / 60, pos % 60);
             
    if (db.addBookmark(current_file, pos, buf)) {
        g_print("Bookmark added: %s\n", buf);
    }
}

void on_bookmark_list_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;

    if (current_file.empty()) return;
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("L:A_N:application_PC:TS_ID:com.kbarni.m4bplayer",
                                                     GTK_WINDOW(window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                     NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *tree_view = gtk_tree_view_new();
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    
    std::vector<Bookmark> bookmarks = db.getBookmarks(current_file);
    for (const auto& b : bookmarks) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, b.name.c_str(), 1, b.position, -1);
    }
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Bookmark", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    gtk_container_add(GTK_CONTAINER(content_area), tree_view);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
        GtkTreeIter iter;
        GtkTreeModel *model;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            int pos;
            gtk_tree_model_get(model, &iter, 1, &pos, -1);
            last_timestamp = pos;
            backend.play_file(current_file.c_str(), last_timestamp);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_chapter_list_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;(void)data;

    if (backend.chapters.empty()) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_INFO,
                                           GTK_BUTTONS_OK,
                                           "No chapters found.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("L:A_N:application_PC:TS_ID:com.kbarni.m4bplayer",
                                                     GTK_WINDOW(window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                     NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 600);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);

    GtkWidget *tree_view = gtk_tree_view_new();
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT64);
    
    for (const auto& ch : backend.chapters) {
        GtkTreeIter iter;
        uint64_t total_seconds = ch.timestamp / 10000000;
        int hours = total_seconds / 3600;
        int minutes = (total_seconds % 3600) / 60;
        int seconds = total_seconds % 60;
        
        char buf[512];
        snprintf(buf, sizeof(buf), "%s (%02d:%02d:%02d)", ch.title.c_str(), hours, minutes, seconds);
        
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, buf, 1, (gint64)ch.timestamp, -1);
    }
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "font", "Sans 14", NULL);
    
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Chapters", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
        GtkTreeIter iter;
        GtkTreeModel *model;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            gint64 ts;
            gtk_tree_model_get(model, &iter, 1, &ts, -1);
            last_timestamp = (int)(ts / 10000000);
            backend.play_file(current_file.c_str(), last_timestamp);
        }
    }
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    init_database();
    if (argc > 1) {
        current_file = argv[1];
        last_timestamp = 0;
    }

    openLipcInstance();
    disableSleep();
    LipcGetIntProperty(lipcInstance,"com.lab126.powerd","flIntensity",&flIntensity);

    LipcSetIntProperty(lipcInstance,"com.lab126.btfd","ensureBTconnection",1);
    LipcSetStringProperty(lipcInstance,"com.lab126.btfd","BTenable","1:1");

    // Window Setup
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request(window, DESKTOP_W_SIZE, DESKTOP_H_SIZE);
    gtk_window_set_title(GTK_WINDOW(window), "L:A_N:application_PC:TS_ID:com.kbarni.m4bplayer");
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    
    GtkRcStyle *style = gtk_widget_get_modifier_style(window);
    style->bg[GTK_STATE_NORMAL].red = 65535;
    style->bg[GTK_STATE_NORMAL].green = 65535;
    style->bg[GTK_STATE_NORMAL].blue = 65535;
    gtk_widget_modify_style(window, style);

    GtkWidget *main_vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    // --- TOP TITLE IMAGE ---
    GdkPixbuf *title_pixbuf = gdk_pixbuf_new_from_inline(-1, title_image, FALSE, NULL);
    GtkWidget *title_image = gtk_image_new_from_pixbuf(title_pixbuf);
    g_object_unref(title_pixbuf);
    
    GtkWidget *title_align = gtk_alignment_new(0.5, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(title_align), title_image);
    gtk_box_pack_start(GTK_BOX(main_vbox), title_align, FALSE, FALSE, 10);


    // --- MIDDLE AREA (Cover, Title, Author, Timer, Play/Pause) ---
    GtkWidget *mid_vbox = gtk_vbox_new(FALSE, 15);
    gtk_box_pack_start(GTK_BOX(main_vbox), mid_vbox, TRUE, TRUE, 0);

    cover_image = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(mid_vbox), cover_image, TRUE, TRUE, 0);

    title_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(title_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(title_label), TRUE);
    gtk_box_pack_start(GTK_BOX(mid_vbox), title_label, FALSE, FALSE, 0);

    artist_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(artist_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(mid_vbox), artist_label, FALSE, FALSE, 0);

    GtkWidget *time_align = gtk_alignment_new(0.5, 0, 0, 0);
    GtkWidget *time_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(time_frame), GTK_SHADOW_ETCHED_IN);
    time_label = gtk_label_new("00:00:00 / 00:00:00");
    
    GtkWidget *time_pad = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(time_pad), 5, 5, 20, 20);
    gtk_container_add(GTK_CONTAINER(time_pad), time_label);
    
    gtk_container_add(GTK_CONTAINER(time_frame), time_pad);
    gtk_container_add(GTK_CONTAINER(time_align), time_frame);
    gtk_box_pack_start(GTK_BOX(mid_vbox), time_align, FALSE, FALSE, 10);
    
    PangoFontDescription *time_font = pango_font_description_from_string("Sans 16");
    gtk_widget_modify_font(time_label, time_font);
    pango_font_description_free(time_font);


    // Playback Controls (RW, Play/Pause, FF)
    GtkWidget *controls_hbox = gtk_hbox_new(FALSE, 20);
    GtkWidget *controls_align = gtk_alignment_new(0.5, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(controls_align), controls_hbox);
    gtk_box_pack_start(GTK_BOX(mid_vbox), controls_align, FALSE, FALSE, 0);

    GtkWidget *rw_btn = create_button_from_icon(fast_rewind_icon, 10);
    gtk_widget_set_size_request(rw_btn, 80, 80);
    g_signal_connect(rw_btn, "clicked", G_CALLBACK(on_rewind_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(controls_hbox), rw_btn, FALSE, FALSE, 0);

    play_pause_btn = create_button_from_icon(play_pause_icon, 10);
    gtk_widget_set_size_request(play_pause_btn, 100, 80);
    g_signal_connect(play_pause_btn, "clicked", G_CALLBACK(on_play_pause_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(controls_hbox), play_pause_btn, FALSE, FALSE, 0);

    GtkWidget *ff_btn = create_button_from_icon(fast_forward_icon, 10);
    gtk_widget_set_size_request(ff_btn, 80, 80);
    g_signal_connect(ff_btn, "clicked", G_CALLBACK(on_ff_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(controls_hbox), ff_btn, FALSE, FALSE, 0);

    GtkWidget *btn_bookmark = create_button_from_icon(bookmark_icon, 10);
    gtk_widget_set_size_request(btn_bookmark, 80, 80);
    g_signal_connect(btn_bookmark, "clicked", G_CALLBACK(on_add_bookmark_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(controls_hbox), btn_bookmark, FALSE, FALSE, 0);

    // --- BOTTOM BUTTONS ---
    GtkWidget *bot_hbox = gtk_hbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), bot_hbox, FALSE, FALSE, 40);

    GtkWidget *btn_open = create_button_from_icon(open_icon, 10);
    gtk_widget_set_size_request(btn_open, 80, 80);
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_dialog_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_open, FALSE, FALSE, 20);

    GtkWidget *btn_history = create_button_from_icon(history_icon, 10);
    gtk_widget_set_size_request(btn_history, 80, 80);
    g_signal_connect(btn_history, "clicked", G_CALLBACK(on_history_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_history, FALSE, FALSE, 0);

    GtkWidget *btn_chapters = create_button_from_icon(chapters_icon, 10);
    gtk_widget_set_size_request(btn_chapters, 80, 80);
    g_signal_connect(btn_chapters, "clicked", G_CALLBACK(on_chapter_list_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_chapters, FALSE, FALSE, 0);

    GtkWidget *btn_bookmark_list = create_button_from_icon(bookmarklist_icon, 10);
    gtk_widget_set_size_request(btn_bookmark_list, 80, 80);
    g_signal_connect(btn_bookmark_list, "clicked", G_CALLBACK(on_bookmark_list_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_bookmark_list, FALSE, FALSE, 0);

    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(bot_hbox), spacer, TRUE, TRUE, 0);

    GtkWidget *btn_light = create_button_from_icon(sunny_icon, 10);
    gtk_widget_set_size_request(btn_light, 80, 80);
    g_signal_connect(btn_light, "clicked", G_CALLBACK(on_fl_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_light, FALSE, FALSE, 0);

    GtkWidget *btn_display = create_button_from_icon(display_icon, 10);
    gtk_widget_set_size_request(btn_display, 80, 80);
    g_signal_connect(btn_display, "clicked", G_CALLBACK(on_displayUpdate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_display, FALSE, FALSE, 0);

    GtkWidget *btn_bt = create_button_from_icon(bluetooth_icon, 10);
    gtk_widget_set_size_request(btn_bt, 80, 80);
    g_signal_connect(btn_bt, "clicked", G_CALLBACK(on_bluetooth_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_bt, FALSE, FALSE, 0);

    GtkWidget *btn_close = create_button_from_icon(close_icon, 10);
    gtk_widget_set_size_request(btn_close, 80, 80);
    g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_hbox), btn_close, FALSE, FALSE, 20);


    gtk_widget_show_all(window);

    if (!current_file.empty()) {
        on_file_open(current_file.c_str());
    }


    g_timeout_add(1000, update_ui, NULL);

    gtk_main();

    return 0;
}
