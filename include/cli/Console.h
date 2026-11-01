#pragma once
// Terminal input/output helpers for the presentation layer.
//
// Console works on std::istream / std::ostream references instead of std::cin /
// std::cout, which lets the automated tests drive the whole CLI with scripted
// input. It never touches the database.
//
// End of input (Ctrl-D, closed pipe) throws InputClosedError so the program can
// exit cleanly instead of looping forever on a dead stdin.

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace csms {

class InputClosedError : public std::runtime_error {
public:
    InputClosedError() : std::runtime_error("Input stream closed") {}
};

// Whole-string integer parse; returns nullopt for "", "12abc", "1.5", overflow.
std::optional<std::int64_t> parseInteger(const std::string& text);

class Console {
public:
    // hideSecrets: switch terminal echo off while a password is typed
    // (POSIX terminals only; ignored elsewhere and for piped input).
    Console(std::istream& in, std::ostream& out, bool hideSecrets = false);

    std::ostream& out() { return out_; }
    void println(const std::string& text = "");

    // Prompts "label: " and returns the trimmed answer (possibly empty).
    std::string ask(const std::string& label);

    // Re-prompts until the answer is a whole number.
    std::int64_t askInt(const std::string& label);

    // Blank answer -> nullopt; otherwise re-prompts until it is a whole number.
    std::optional<std::int64_t> askOptionalInt(const std::string& label);

    // Re-prompts until the answer starts with y/Y or n/N.
    bool confirm(const std::string& question);

    // Like ask() but the text is not trimmed and (on a terminal) not echoed.
    std::string askSecret(const std::string& label);

    // Asks twice and requires both entries to match; throws ValidationError otherwise.
    std::string askNewPassword();

    // Shows a numbered menu plus "0) zeroLabel" and returns the chosen number
    // (0 = zeroLabel). Re-prompts on invalid input.
    int askChoice(const std::string& title, const std::vector<std::string>& options,
                  const std::string& zeroLabel = "Back");

    // Runs an action; domain errors (CsmsError) and other exceptions are printed
    // as readable messages instead of crashing. InputClosedError passes through.
    void runGuarded(const std::function<void()>& action);

    // True when stdin is an interactive terminal.
    static bool stdinIsTerminal();

private:
    std::string readLine(const std::string& prompt);

    std::istream& in_;
    std::ostream& out_;
    bool hideSecrets_;
};

} // namespace csms
