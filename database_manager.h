#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <sqlite3.h>

struct Book {
    int id;
    std::string filepath;
    int last_position;
    int duration;
    bool is_finished;
    int last_played_at;
};

struct Bookmark {
    int id;
    int book_id;
    int position;
    std::string name;
    int created_at;
};

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Initialization
    bool init(const std::string& db_path);

    // Settings
    bool setSetting(const std::string& key, const std::string& value);
    std::string getSetting(const std::string& key, const std::string& default_value = "");

    // Book Progress
    // Returns true on success
    bool updateBookProgress(const std::string& filepath, int position, int duration, bool is_finished);
    // Returns true if book found and populated
    bool getBookProgress(const std::string& filepath, Book& out_book);
    std::string getLastPlayedFile();

    // Bookmarks
    bool addBookmark(const std::string& filepath, int position, const std::string& name);
    std::vector<Bookmark> getBookmarks(const std::string& filepath);
    bool deleteBookmark(int bookmark_id);

    // History
    std::vector<Book> getHistory();

    // Migration
    bool migrateFromLegacy(const std::string& legacy_path);

private:
    sqlite3* db;
    bool executeQuery(const std::string& query);
    int getBookId(const std::string& filepath);
};

#endif // DATABASE_MANAGER_H
