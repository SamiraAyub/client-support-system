#include "cli/Console.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>

#include "common/Exceptions.h"
#include "common/Validation.h"

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#define CSMS_HAS_TERMIOS 1
#endif

namespace csms {

std::optional<std::int64_t> parseInteger(const std::string& text) {
    if (text.empty()) return std::nullopt;
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0') return std::nullopt;
    return static_cast<std::int64_t>(value);
}

#ifdef CSMS_HAS_TERMIOS
namespace {
// Switches terminal echo off and restores it on scope exit (RAII).
class EchoOff {
public:
    EchoOff() {
        active_ = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved_) == 0;
        if (active_) {
            termios quiet = saved_;
            quiet.c_lflag &= static_cast<tcflag_t>(~ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
        }
    }
    ~EchoOff() {
        if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
    }
    EchoOff(const EchoOff&) = delete;
    EchoOff& operator=(const EchoOff&) = delete;
private:
    termios saved_{};
    bool active_ = false;
};
} // namespace
#endif

Console::Console(std::istream& in, std::ostream& out, bool hideSecrets)
    : in_(in), out_(out), hideSecrets_(hideSecrets) {}

bool Console::stdinIsTerminal() {
#ifdef CSMS_HAS_TERMIOS
    return isatty(STDIN_FILENO) != 0;
#else
    return false;
#endif
}

void Console::println(const std::string& text) {
    out_ << text << '\n';
}

std::string Console::readLine(const std::string& prompt) {
    out_ << prompt << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        throw InputClosedError();
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();   // Windows line endings
    return line;
}

std::string Console::ask(const std::string& label) {
    return trim(readLine(label + ": "));
}

std::int64_t Console::askInt(const std::string& label) {
    for (;;) {
        if (const auto value = parseInteger(ask(label))) return *value;
        println("Please enter a whole number.");
    }
}

std::optional<std::int64_t> Console::askOptionalInt(const std::string& label) {
    for (;;) {
        const std::string answer = ask(label + " (blank = any)");
        if (answer.empty()) return std::nullopt;
        if (const auto value = parseInteger(answer)) return *value;
        println("Please enter a whole number, or leave blank.");
    }
}

bool Console::confirm(const std::string& question) {
    for (;;) {
        const std::string answer = ask(question + " (y/n)");
        if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) return true;
        if (!answer.empty() && (answer[0] == 'n' || answer[0] == 'N')) return false;
        println("Please answer y or n.");
    }
}

std::string Console::askSecret(const std::string& label) {
#ifdef CSMS_HAS_TERMIOS
    if (hideSecrets_) {
        std::string line;
        {
            EchoOff guard;
            line = readLine(label + ": ");
        }
        out_ << '\n';   // the Enter key was not echoed
        return line;
    }
#endif
    return readLine(label + ": ");
}

std::string Console::askNewPassword() {
    const std::string first = askSecret("New password");
    const std::string second = askSecret("Repeat new password");
    if (first != second) {
        throw ValidationError("The two passwords do not match");
    }
    return first;
}

int Console::askChoice(const std::string& title, const std::vector<std::string>& options,
                       const std::string& zeroLabel) {
    for (;;) {
        println();
        println("=== " + title + " ===");
        for (std::size_t i = 0; i < options.size(); ++i) {
            println("  " + std::to_string(i + 1) + ") " + options[i]);
        }
        println("  0) " + zeroLabel);

        const auto choice = parseInteger(ask("Choose"));
        if (choice && *choice >= 0 && *choice <= static_cast<std::int64_t>(options.size())) {
            return static_cast<int>(*choice);
        }
        println("Invalid choice. Enter a number from 0 to " + std::to_string(options.size()) + ".");
    }
}

void Console::runGuarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const InputClosedError&) {
        throw;   // not an error to report: the whole program is ending
    } catch (const CsmsError& e) {
        out_ << "\nError: " << e.what() << '\n';
    } catch (const std::exception& e) {
        out_ << "\nUnexpected error: " << e.what() << '\n';
    }
}

} // namespace csms
