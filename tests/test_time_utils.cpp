#include <gtest/gtest.h>

#include <regex>

#include "common/TimeUtils.h"

using namespace csms;

TEST(TimeUtilsTest, FormatsEpochAsIso8601Utc) {
    EXPECT_EQ(formatTimestampUtc(0), "1970-01-01T00:00:00Z");
}

TEST(TimeUtilsTest, FormatsKnownTimestamp) {
    EXPECT_EQ(formatTimestampUtc(1700000000), "2023-11-14T22:13:20Z");
}

TEST(TimeUtilsTest, CurrentTimestampMatchesDocumentedFormat) {
    const std::regex pattern(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    EXPECT_TRUE(std::regex_match(currentTimestampUtc(), pattern));
}

TEST(TimeUtilsTest, TimestampsSortChronologicallyAsText) {
    EXPECT_LT(formatTimestampUtc(1700000000), formatTimestampUtc(1700000001));
    EXPECT_LT(formatTimestampUtc(999999999), formatTimestampUtc(1000000000));
}
