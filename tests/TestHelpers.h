#pragma once
// Shared test utilities.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "database/Database.h"

namespace csms::test_support {

// Base fixture: a fresh in-memory database with the schema already created.
class InMemoryDbTest : public ::testing::Test {
protected:
    void SetUp() override { db.initializeSchema(); }
    Database db{":memory:"};
};

// A unique database path in the system temp directory, deleted on destruction.
// Tests never touch the real data/ directory.
class TempDbPath {
public:
    TempDbPath() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = (std::filesystem::temp_directory_path() /
                 ("csms_test_" + std::to_string(stamp) + ".db")).string();
    }
    ~TempDbPath() {
        for (const char* suffix : {"", "-journal", "-wal", "-shm"}) {
            std::remove((path_ + suffix).c_str());
        }
    }
    TempDbPath(const TempDbPath&) = delete;
    TempDbPath& operator=(const TempDbPath&) = delete;

    const std::string& str() const { return path_; }

private:
    std::string path_;
};

} // namespace csms::test_support
