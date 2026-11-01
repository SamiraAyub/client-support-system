#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "common/Exceptions.h"
#include "common/Validation.h"

using namespace csms;

TEST(TrimTest, RemovesSurroundingWhitespaceOnly) {
    EXPECT_EQ(trim("  hello world \t\n"), "hello world");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
}

// ----------------------------------------------------------------------- name

TEST(ValidateNameTest, AcceptsAndTrimsNormalNames) {
    EXPECT_EQ(validateName("  Ann Lee  "), "Ann Lee");
    EXPECT_EQ(validateName("Zoë O'Brien-Smith"), "Zoë O'Brien-Smith");
}

TEST(ValidateNameTest, RejectsEmptyAndWhitespaceOnly) {
    EXPECT_THROW(validateName(""), ValidationError);
    EXPECT_THROW(validateName("   \t "), ValidationError);
}

TEST(ValidateNameTest, RejectsTooLongNames) {
    EXPECT_NO_THROW(validateName(std::string(kMaxNameLength, 'a')));
    EXPECT_THROW(validateName(std::string(kMaxNameLength + 1, 'a')), ValidationError);
}

TEST(ValidateNameTest, RejectsControlCharacters) {
    EXPECT_THROW(validateName("Ann\nLee"), ValidationError);
    EXPECT_THROW(validateName(std::string("Ann\0Lee", 7)), ValidationError);
}

// ---------------------------------------------------------------------- email

TEST(ValidateEmailTest, AcceptsValidAddresses) {
    const std::vector<std::string> valid = {
        "ann@example.com",
        "first.last+tag@sub.example.co.uk",
        "o'brien@example.ie",
        "A@B.CO",
        "user_name@my-domain.org",
    };
    for (const auto& e : valid) {
        EXPECT_NO_THROW(validateEmail(e)) << e;
    }
}

TEST(ValidateEmailTest, TrimsWhitespace) {
    EXPECT_EQ(validateEmail("  ann@example.com \n"), "ann@example.com");
}

TEST(ValidateEmailTest, RejectsInvalidAddresses) {
    const std::vector<std::string> invalid = {
        "",
        "   ",
        "plainaddress",
        "a@b",               // no dot in domain
        "a@@b.com",          // two @
        "a@b@c.com",
        "@b.com",            // empty local part
        "a@",                // empty domain
        "a@.com",            // empty label
        "a@b..com",          // empty label
        "a@b.com.",          // trailing dot
        "a@b.c",             // one-letter TLD
        "a@-b.com",          // label starts with hyphen
        "a@b-.com",          // label ends with hyphen
        "a b@c.com",         // space
        "user@exa mple.com",
        ".a@b.com",          // leading dot in local part
        "a.@b.com",          // trailing dot in local part
        "a..b@c.com",        // consecutive dots
        "a(b)@c.com",        // illegal characters
        "é@example.com",     // non-ASCII
    };
    for (const auto& e : invalid) {
        EXPECT_THROW(validateEmail(e), ValidationError) << "'" << e << "'";
    }
}

TEST(ValidateEmailTest, EnforcesLengthLimits) {
    const std::string longLocal(65, 'a');
    EXPECT_THROW(validateEmail(longLocal + "@example.com"), ValidationError);

    const std::string tooLong = std::string(60, 'a') + "@" + std::string(250, 'b') + ".com";
    EXPECT_THROW(validateEmail(tooLong), ValidationError);
}

// ---------------------------------------------------------------------- phone

TEST(ValidatePhoneTest, AcceptsCommonFormats) {
    const std::vector<std::string> valid = {
        "555-123-4567",
        "+1 (555) 123-4567",
        "0123456",
        "+8801712345678",
        "  5551234567  ",
    };
    for (const auto& p : valid) {
        EXPECT_NO_THROW(validatePhone(p)) << p;
    }
}

TEST(ValidatePhoneTest, TrimsWhitespace) {
    EXPECT_EQ(validatePhone("  555-123-4567 "), "555-123-4567");
}

TEST(ValidatePhoneTest, RejectsInvalidNumbers) {
    const std::vector<std::string> invalid = {
        "",
        "abc",
        "123",                    // too few digits
        "12-34",
        "555+1234567",            // '+' not at the start
        "+",
        "1234567890123456",       // 16 digits
        "555-123-4567 ext 9",     // letters
        "555.123.4567",           // dots not allowed
    };
    for (const auto& p : invalid) {
        EXPECT_THROW(validatePhone(p), ValidationError) << "'" << p << "'";
    }
}
