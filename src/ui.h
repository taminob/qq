#ifndef QQ_UI_H
#define QQ_UI_H

#include <expected>
#include <functional>
#include <iostream>
#include <mutex>
#include <ostream>

#include <ppplugin/shell/shell_type_conversion.h>

class UiStream {
public:
    explicit UiStream(std::ostream* stream, auto start_callback, auto end_callback)
        : stream_ { stream }
        , start_callback_ { std::move(start_callback) }
        , end_callback_ { std::move(end_callback) }
    {
        if (stream_ != nullptr && start_callback_) {
            start_callback_();
        }
    }
    ~UiStream()
    {
        if (stream_ == nullptr) {
            // do not call callback if output is disabled
            return;
        }
        if (end_callback_) {
            end_callback_();
        }
    }
    UiStream(const UiStream&) = delete;
    UiStream(UiStream&&) = delete;
    UiStream& operator=(const UiStream&) = delete;
    UiStream& operator=(UiStream&&) = delete;

    template <typename T>
    UiStream& operator<<(T&& value)
    {
        if (stream_) {
            *stream_ << std::forward<T>(value);
        }
        return *this;
    }

    void flush()
    {
        if (stream_ != nullptr) {
            stream_->flush();
        }
    }

private:
    std::ostream* stream_;
    std::move_only_function<void()> start_callback_;
    std::move_only_function<void()> end_callback_;
};

class Ui {
public:
    Ui() = default;

    ~Ui() = default;
    Ui(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui& operator=(Ui&&) = delete;

    /**
     * Return error stream.
     */
    UiStream error()
    {
        return UiStream { &std::cerr, [lock = std::unique_lock { output_mutex_ }]() { }, nullptr };
    }

    /**
     * Return stream that is used to emit warnings.
     */
    UiStream warn()
    {
        return error();
    }

    UiStream output()
    {
        return UiStream { &std::cout, [lock = std::unique_lock { output_mutex_ }]() { }, nullptr };
    }

    UiStream debug()
    {
        if (!enable_debug_) {
            return dummy();
        }
        return UiStream { &std::cout, [lock = std::unique_lock { output_mutex_ }]() { }, nullptr };
    }

    /**
     * Return index in container to selected choice.
     */
    template <typename Container>
    std::size_t userChoice(Container&& choices)
    {
        auto out = output();
        std::size_t num { 1 };
        constexpr std::size_t DEFAULT_USER_CHOICE = 1;
        for (auto&& choice : choices) {
            out << num << (num == DEFAULT_USER_CHOICE ? "*" : "") << ") " << choice << '\n';
            ++num;
        }
        out << "> ";
        out.flush();
        while (true) {
            auto selection = input<std::size_t>();
            if (selection) {
                if (*selection > 0 && *selection <= choices.size()) {
                    return *selection - 1;
                }
            } else if (selection.error().empty()) {
                // no user input, use default
                static_assert(DEFAULT_USER_CHOICE > 0, "Indices of user choice start at 1");
                return DEFAULT_USER_CHOICE - 1;
            }
            out << "Invalid input, try again! > ";
            out.flush();
        }
    }

    /**
     * Read user input of given type.
     *
     * @return value on success, otherwise the raw input string
     */
    template <typename T>
    std::expected<T, std::string> input()
    {
        std::string line;
        std::getline(std::cin, line);
        if (auto&& value = ppplugin::convertFromShellString<T>(line)) {
            return *value;
        }
        return std::unexpected { line };
    }

    void enableDebug(bool enable = true)
    {
        enable_debug_ = enable;
    }

private:
    static UiStream dummy()
    {
        return UiStream { nullptr, nullptr, nullptr };
    }

private:
    std::mutex output_mutex_;
    bool enable_debug_ { false };
};

#endif // QQ_UI_H
