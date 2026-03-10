#include "database_manager.h"
//#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>

DatabaseManager::DatabaseManager() : db(nullptr) {}

DatabaseManager::~DatabaseManager() {
    if (db) {
        sqlite3_close(db);
    }
}

bool DatabaseManager::init(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc) {
        printf("Can't open database: %s\n", sqlite3_errmsg(db));
        return false;
    }

    const char* sql_settings = 
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT);";
    
    const char* sql_books = 
        "CREATE TABLE IF NOT EXISTS books ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "filepath TEXT UNIQUE NOT NULL, "
        "last_position INTEGER DEFAULT 0, "
        "duration INTEGER DEFAULT 0, "
        "is_finished INTEGER DEFAULT 0, "
        "last_played_at INTEGER);";

    const char* sql_bookmarks = 
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "book_id INTEGER, "
        "position INTEGER NOT NULL, "
        "name TEXT, "
        "created_at INTEGER, "
        "FOREIGN KEY(book_id) REFERENCES books(id));";

    if (!executeQuery(sql_settings)) return false;
    if (!executeQuery(sql_books)) return false;
    if (!executeQuery(sql_bookmarks)) return false;

    return true;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    char* zErrMsg = 0;
    int rc = sqlite3_exec(db, query.c_str(), 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

bool DatabaseManager::setSetting(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::string DatabaseManager::getSetting(const std::string& key, const std::string& default_value) {
    const char* sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return default_value;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    std::string result = default_value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* val = sqlite3_column_text(stmt, 0);
        if (val) result = std::string((const char*)val);
    }
    sqlite3_finalize(stmt);
    return result;
}

int DatabaseManager::getBookId(const std::string& filepath) {
    const char* sql = "SELECT id FROM books WHERE filepath = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);

    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

bool DatabaseManager::updateBookProgress(const std::string& filepath, int position, int duration, bool is_finished) {
    int id = getBookId(filepath);
    int current_time = (int)time(NULL);

    if (id > 0) {
        const char* sql = "UPDATE books SET last_position = ?, duration = ?, is_finished = ?, last_played_at = ? WHERE id = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, position);
        sqlite3_bind_int(stmt, 2, duration);
        sqlite3_bind_int(stmt, 3, is_finished ? 1 : 0);
        sqlite3_bind_int(stmt, 4, current_time);
        sqlite3_bind_int(stmt, 5, id);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    } else {
        const char* sql = "INSERT INTO books (filepath, last_position, duration, is_finished, last_played_at) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, position);
        sqlite3_bind_int(stmt, 3, duration);
        sqlite3_bind_int(stmt, 4, is_finished ? 1 : 0);
        sqlite3_bind_int(stmt, 5, current_time);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
}

bool DatabaseManager::getBookProgress(const std::string& filepath, Book& out_book) {
    const char* sql = "SELECT id, last_position, duration, is_finished, last_played_at FROM books WHERE filepath = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, filepath.c_str(), -1, SQLITE_STATIC);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_book.id = sqlite3_column_int(stmt, 0);
        out_book.filepath = filepath;
        out_book.last_position = sqlite3_column_int(stmt, 1);
        out_book.duration = sqlite3_column_int(stmt, 2);
        out_book.is_finished = sqlite3_column_int(stmt, 3) != 0;
        out_book.last_played_at = sqlite3_column_int(stmt, 4);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

std::string DatabaseManager::getLastPlayedFile() {
    return getSetting("last_opened_file", "");
}

bool DatabaseManager::addBookmark(const std::string& filepath, int position, const std::string& name) {
    int book_id = getBookId(filepath);
    if (book_id == 0) {
        // Create book entry if it doesn't exist
        if (!updateBookProgress(filepath, 0, 0, false)) return false;
        book_id = getBookId(filepath);
        if (book_id == 0) return false;
    }

    const char* sql = "INSERT INTO bookmarks (book_id, position, name, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, book_id);
    sqlite3_bind_int(stmt, 2, position);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Bookmark> DatabaseManager::getBookmarks(const std::string& filepath) {
    std::vector<Bookmark> bookmarks;
    int book_id = getBookId(filepath);
    if (book_id == 0) return bookmarks;

    const char* sql = "SELECT id, position, name, created_at FROM bookmarks WHERE book_id = ? ORDER BY position ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return bookmarks;

    sqlite3_bind_int(stmt, 1, book_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Bookmark b;
        b.id = sqlite3_column_int(stmt, 0);
        b.book_id = book_id;
        b.position = sqlite3_column_int(stmt, 1);
        const unsigned char* name_val = sqlite3_column_text(stmt, 2);
        b.name = name_val ? std::string((const char*)name_val) : "";
        b.created_at = sqlite3_column_int(stmt, 3);
        bookmarks.push_back(b);
    }
    sqlite3_finalize(stmt);
    return bookmarks;
}

bool DatabaseManager::deleteBookmark(int bookmark_id) {
    const char* sql = "DELETE FROM bookmarks WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, bookmark_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Book> DatabaseManager::getHistory() {
    std::vector<Book> history;
    const char* sql = "SELECT id, filepath, last_position, duration, is_finished, last_played_at FROM books ORDER BY last_played_at DESC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return history;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Book b;
        b.id = sqlite3_column_int(stmt, 0);
        const unsigned char* fp = sqlite3_column_text(stmt, 1);
        b.filepath = fp ? std::string((const char*)fp) : "";
        b.last_position = sqlite3_column_int(stmt, 2); 
        b.duration = sqlite3_column_int(stmt, 3);
        b.is_finished = sqlite3_column_int(stmt, 4) != 0;
        b.last_played_at = sqlite3_column_int(stmt, 5);
        history.push_back(b);
    }
    sqlite3_finalize(stmt);
    return history;
}

bool DatabaseManager::migrateFromLegacy(const std::string& legacy_path) {
    std::ifstream in(legacy_path);
    if (!in.is_open()) return false;

    std::string line;
    // First line is current file
    if (std::getline(in, line)) {
        if (line != "NONE" && !line.empty()) {
            setSetting("last_opened_file", line);
        }
    }

    // Subsequent lines are history: filepath|timestamp
    while (std::getline(in, line)) {
        size_t delimiter = line.find('|');
        if (delimiter != std::string::npos) {
            std::string filepath = line.substr(0, delimiter);
            try {
                int position = std::stoi(line.substr(delimiter + 1));
                // We don't have duration or is_finished, so default to 0/false.
                // We also don't have last_played_at, but we can set it to now or 0. 
                // Since this is migration, maybe 0 so it doesn't mess up "recently played" too much if we open a new file. 
                // But updateBookProgress sets it to NOW.
                // Let's use a specialized update or just accept it sets to NOW.
                // If we iterate through them, they all get "NOW". The order might be preserved if we insert them in order?
                // Legacy file order isn't guaranteed to be sorted by time, but typically map iteration order in C++ std::map is by key (filepath).
                // So we lose time information. That's fine.
                updateBookProgress(filepath, position, 0, false);
            } catch (...) {
                // Ignore parse errors
            }
        }
    }
    in.close();

    // Rename legacy file
    std::string backup_path = legacy_path + ".bak";
    std::rename(legacy_path.c_str(), backup_path.c_str());

    return true;
}
