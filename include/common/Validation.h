#pragma once
// Input validation shared by the service layer.
//
// Each validateXxx() function trims surrounding whitespace, checks the value and
// returns the cleaned string. On failure it throws ValidationError with a
// message that is safe to show directly to the user.
//
// Rules (documented in docs/requirements.md):
//   name  : 1-100 bytes after trimming, no control characters
//   email : practical check - one '@', sane local part, dotted domain; max 254
//   phone : digits with optional leading '+', spaces, '-', '(' and ')';
//           7-15 digits (the E.164 maximum)
// Email format checking is deliberately pragmatic, not a full RFC 5322 parser.

#include <cstddef>
#include <string>

namespace csms {

constexpr std::size_t kMaxNameLength = 100;
constexpr std::size_t kMaxEmailLength = 254;
constexpr std::size_t kMaxTitleLength = 150;
constexpr std::size_t kMaxDescriptionLength = 2000;
constexpr std::size_t kMaxNoteLength = 500;
constexpr std::size_t kMinPasswordLength = 8;
constexpr std::size_t kMaxPasswordLength = 128;

std::string trim(const std::string& text);

std::string validateName(const std::string& raw);
std::string validateEmail(const std::string& raw);
std::string validatePhone(const std::string& raw);

// Free text. `fieldName` is used in error messages ("Title must not be empty").
// Both trim and reject control characters; the optional variant allows "".
std::string validateRequiredText(const std::string& fieldName, const std::string& raw,
                                 std::size_t maxLength);
std::string validateOptionalText(const std::string& fieldName, const std::string& raw,
                                 std::size_t maxLength);

// Usernames: 3-32 characters, letters/digits/'.'/'_'/'-', starting with a letter or digit.
std::string validateUsername(const std::string& raw);

// Passwords are NOT trimmed (spaces may be intentional). 8-128 characters, no
// control characters. Returns the password unchanged.
std::string validatePassword(const std::string& raw);

} // namespace csms
