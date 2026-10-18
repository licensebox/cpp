#ifndef LICENSEBOX_CLIENT_HPP
#define LICENSEBOX_CLIENT_HPP
#include <cstring>
#include <exception>
#include <functional>
#include <licensebox.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace licensebox {
class Error : public std::runtime_error {
public:
  const lb_result result;
  const std::string code;
  Error(lb_result result, std::string code, const std::string &detail)
      : std::runtime_error("licensebox: " + detail), result(result),
        code(std::move(code)) {}
};
struct Config {
  std::string product;
  std::map<std::string, std::string> keys;
  std::string version, server, directory;
  std::function<void(const std::string &, const std::string &)> on_sign_in;
  std::function<double()> clock;
  // Platform/test seam; never supply an identity from the end user.
  std::function<std::string()> machine_binding;
  std::function<bool()> cancelled;
};

// RAII wrapper over the shared C implementation. All returned JSON strings own
// their data. Thread serialization, persistence and policy handling live in C.
class Client {
  Config config_;
  lb_client *handle_ = nullptr;
  std::exception_ptr callback_error_;
  std::mutex mutex_;

  static void on_sign_in(void *p, const char *url, const char *code) noexcept {
    auto &self = *static_cast<Client *>(p);
    try {
      self.config_.on_sign_in(url, code);
    } catch (...) {
      self.callback_error_ = std::current_exception();
    }
  }
  static double clock(void *p) noexcept {
    auto &self = *static_cast<Client *>(p);
    try {
      return self.config_.clock();
    } catch (...) {
      self.callback_error_ = std::current_exception();
      return 0;
    }
  }
  static int binding(void *p, char *out, std::size_t capacity) noexcept {
    auto &self = *static_cast<Client *>(p);
    try {
      auto value = self.config_.machine_binding();
      if (value.size() >= capacity)
        return 1;
      std::memcpy(out, value.c_str(), value.size() + 1);
      return 0;
    } catch (...) {
      self.callback_error_ = std::current_exception();
      return 1;
    }
  }
  static int cancelled(void *p) noexcept {
    auto &self = *static_cast<Client *>(p);
    if (self.callback_error_)
      return 1;
    try {
      return self.config_.cancelled && self.config_.cancelled();
    } catch (...) {
      self.callback_error_ = std::current_exception();
      return 1;
    }
  }
  std::string checked(lb_result result, char *output) {
    std::unique_ptr<char, decltype(&lb_free)> owned(output, lb_free);
    if (callback_error_) {
      auto error = callback_error_;
      callback_error_ = nullptr;
      std::rethrow_exception(error);
    }
    if (result != LB_OK)
      throw Error(result, lb_error_code(handle_), lb_error_message(handle_));
    return output ? output : "";
  }
  template <class F> std::string call(F fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    char *output = nullptr;
    auto result = fn(&output);
    return checked(result, output);
  }

public:
  explicit Client(Config config) : config_(std::move(config)) {
    std::vector<lb_public_key> keys;
    for (const auto &[id, value] : config_.keys)
      keys.push_back({id.c_str(), value.c_str()});
    lb_config options{};
    options.product = config_.product.c_str();
    options.keys = keys.data();
    options.key_count = keys.size();
    options.version = config_.version.c_str();
    options.server = config_.server.empty() ? nullptr : config_.server.c_str();
    options.directory =
        config_.directory.empty() ? nullptr : config_.directory.c_str();
    options.context = this;
    options.cancelled = cancelled;
    if (config_.on_sign_in)
      options.on_sign_in = on_sign_in;
    if (config_.clock)
      options.clock = clock;
    if (config_.machine_binding)
      options.machine_binding = binding;
    auto result = lb_client_create(&options, &handle_);
    if (result != LB_OK)
      throw Error(result, lb_result_code(result), "cannot create client");
  }
  ~Client() { lb_client_destroy(handle_); }
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;
  Client(Client &&) = delete;
  Client &operator=(Client &&) = delete;

  std::string lease() {
    return call([&](char **out) { return lb_lease(handle_, out); });
  }
  std::string require(const std::string &module) {
    return call(
        [&](char **out) { return lb_require(handle_, module.c_str(), out); });
  }
  std::string status() {
    return call([&](char **out) { return lb_status(handle_, out); });
  }
  std::string sign_in(double timeout_seconds = 0) {
    return call(
        [&](char **out) { return lb_sign_in(handle_, timeout_seconds, out); });
  }
  void logout() {
    std::lock_guard<std::mutex> lock(mutex_);
    checked(lb_logout(handle_), nullptr);
  }
  std::string modules() {
    return call([&](char **out) { return lb_modules(handle_, out); });
  }
  std::string requests() {
    return call([&](char **out) { return lb_requests(handle_, out); });
  }
  std::string access_link() {
    return call([&](char **out) { return lb_access_link(handle_, out); });
  }
  std::string request_access(const std::vector<std::string> &modules,
                             const std::string &note) {
    std::vector<const char *> names;
    for (const auto &name : modules)
      names.push_back(name.c_str());
    return call([&](char **out) {
      return lb_request_access(handle_, names.data(), names.size(),
                               note.c_str(), out);
    });
  }
  void record(const std::string &command, bool success,
              double elapsed_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    checked(lb_record(handle_, command.c_str(), success, elapsed_seconds),
            nullptr);
  }
};
} // namespace licensebox
#endif
