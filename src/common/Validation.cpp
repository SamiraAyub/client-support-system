#include "common/Validation.h"

#include <cctype>
#include <cstring>

#include "common/Exceptions.h"

namespace csms {

namespace {

bool hasControlCharacter(const std::string& s) {
    for (const unsigned char c : s) {
        if (c < 0x20 || c == 0x7F) return true;
    }
    return false;
}

bool isAlnum(unsigned char c) { return std::isalnum(c) != 0; }

// Characters allowed in the local part (before '@') besides letters and digits.
bool isLocalPartSymbol(unsigned char c) {
    return std::strchr("!#$%&'*+/=?^_`{|}~.-", c) != nullptr;
}

void validateDomain(const std::string& domain) {
    const ValidationError invalid("Email domain is not valid");

    std::size_t labelCount = 0;
    std::size_t start = 0;
    std::string lastLabel;
    while (start <= domain.size()) {
        std::size_t dot = domain.find('.', start);
        if (dot == std::string::npos) dot = domain.size();
        const std::string label = domain.substr(start, dot - start);

        if (label.empty() || label.size() > 63) throw invalid;
        if (label.front() == '-' || label.back() == '-') throw invalid;
        for (const unsigned char c : label) {
            if (!isAlnum(c) && c != '-') throw invalid;
        }
        ++labelCount;
        lastLabel = label;
        start = dot + 1;
    }
    if (labelCount < 2 || lastLabel.size() < 2) throw invalid;
}

} // namespace

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

std::string validateName(const std::string& raw) {
    const std::string value = trim(raw);
    if (value.empty()) {
        throw ValidationError("Name must not be empty");
    }
    if (value.size() > kMaxNameLength) {
        throw ValidationError("Name must be at most " + std::to_string(kMaxNameLength) +
                              " characters");
    }
    if (hasControlCharacter(value)) {
        throw ValidationError("Name contains invalid characters");
    }
    return value;
}

std::string validateEmail(const std::string& raw) {
    const std::string value = trim(raw);
    if (value.empty()) {
        throw ValidationError("Email must not be empty");
    }
    if (value.size() > kMaxEmailLength) {
        throw ValidationError("Email must be at most " + std::to_string(kMaxEmailLength) +
                              " characters");
    }
    for (const unsigned char c : value) {
        if (c <= 0x20 || c >= 0x7F) {
            throw ValidationError("Email must not contain spaces or non-ASCII characters");
        }
    }

    const std::size_t at = value.find('@');
    if (at == std::string::npos || at != value.rfind('@')) {
        throw ValidationError("Email must contain exactly one '@'");
    }

    const std::string local = value.substr(0, at);
    const std::string domain = value.substr(at + 1);

    if (local.empty() || local.size() > 64) {
        throw ValidationError("Email local part (before '@') must be 1-64 characters");
    }
    if (local.front() == '.' || local.back() == '.' || local.find("..") != std::string::npos) {
        throw ValidationError("Email local part has misplaced dots");
    }
    for (const unsigned char c : local) {
        if (!isAlnum(c) && !isLocalPartSymbol(c)) {
            throw ValidationError("Email local part contains an invalid character");
        }
    }

    validateDomain(domain);
    return value;
}

std::string validatePhone(const std::string& raw) {
    const std::string value = trim(raw);
    if (value.empty()) {
        throw ValidationError("Phone number must not be empty");
    }

    std::size_t digits = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isdigit(c)) {
            ++digits;
        } else if (c == ' ' || c == '-' || c == '(' || c == ')') {
            // allowed separators
        } else if (c == '+' && i == 0) {
            // allowed only as the leading international prefix
        } else {
            throw ValidationError(
                "Phone number may contain only digits, spaces, '-', '(', ')' and a leading '+'");
        }
    }
    if (digits < 7 || digits > 15) {
        throw ValidationError("Phone number must contain between 7 and 15 digits");
    }
    return value;
}

std::string validateRequiredText(const std::string& fieldName, const std::string& raw,
                                 std::size_t maxLength) {
    const std::string value = trim(raw);
    if (value.empty()) {
        throw ValidationError(fieldName + " must not be empty");
    }
    if (value.size() > maxLength) {
        throw ValidationError(fieldName + " must be at most " + std::to_string(maxLength) +
                              " characters");
    }
    if (hasControlCharacter(value)) {
        throw ValidationError(fieldName + " contains invalid characters");
    }
    return value;
}

std::string validateOptionalText(const std::string& fieldName, const std::string& raw,
                                 std::size_t maxLength) {
    const std::string value = trim(raw);
    if (value.empty()) return value;
    return validateRequiredText(fieldName, value, maxLength);
}

std::string validateUsername(const std::string& raw) {
    const std::string value = trim(raw);
    if (value.size() < 3 || value.size() > 32) {
        throw ValidationError("Username must be 3-32 characters");
    }
    if (!isAlnum(static_cast<unsigned char>(value.front()))) {
        throw ValidationError("Username must start with a letter or digit");
    }
    for (const unsigned char c : value) {
        if (!isAlnum(c) && c != '.' && c != '_' && c != '-') {
            throw ValidationError("Username may contain only letters, digits, '.', '_' and '-'");
        }
    }
    return value;
}

std::string validatePassword(const std::string& raw) {
    if (raw.size() < kMinPasswordLength) {
        throw ValidationError("Password must be at least " + std::to_string(kMinPasswordLength) +
                              " characters");
    }
    if (raw.size() > kMaxPasswordLength) {
        throw ValidationError("Password must be at most " + std::to_string(kMaxPasswordLength) +
                              " characters");
    }
    if (hasControlCharacter(raw)) {
        throw ValidationError("Password contains invalid characters");
    }
    return raw;
}

} // namespace csms
