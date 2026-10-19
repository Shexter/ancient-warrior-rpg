#pragma once

// JSON reading helpers shared by the data loaders (content and world). Each records errors into
// a ContentError list instead of throwing, so a loader can report every problem in one pass.

#include "game/content_loader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace heroes::game::detail {

using Json = nlohmann::json;
using Errors = std::vector<ContentError>;

struct Ctx {
    Errors& errors;
    std::string file;
};

template <class T> inline bool convert(const Json& v, T& out) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (!v.is_string())
            return false;
        out = v.get<std::string>();
        return true;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!v.is_boolean())
            return false;
        out = v.get<bool>();
        return true;
    } else {
        if (!v.is_number_integer())
            return false;
        if (v.is_number_unsigned()) {
            const auto u = v.get<std::uint64_t>();
            if (!std::in_range<T>(u))
                return false;
            out = static_cast<T>(u);
        } else {
            const auto s = v.get<std::int64_t>();
            if (!std::in_range<T>(s))
                return false;
            out = static_cast<T>(s);
        }
        return true;
    }
}

// Reads the keys of one JSON object, recording type, range, missing and unknown-key errors.
class Fields {
  public:
    Fields(Ctx& ctx, const Json& j, std::string entry,
           std::initializer_list<std::string_view> allowed, std::string prefix = "")
        : ctx_(ctx), json_(j), entry_(std::move(entry)), prefix_(std::move(prefix)) {
        if (!j.is_object()) {
            fail("expected an object");
            return;
        }
        for (const auto& [key, value] : j.items()) {
            if (std::ranges::find(allowed, std::string_view(key)) == allowed.end())
                fail(std::format("unknown key '{}{}'", prefix_, key));
        }
    }

    void fail(std::string message) {
        ctx_.errors.push_back({ctx_.file, entry_, std::move(message)});
    }
    Ctx& ctx() { return ctx_; }
    const std::string& entry() const { return entry_; }

    const Json* raw(std::string_view key) const {
        if (!json_.is_object())
            return nullptr;
        auto it = json_.find(key);
        return it == json_.end() ? nullptr : &*it;
    }

    template <class T>
    T req(std::string_view key, T lo = std::numeric_limits<T>::lowest(),
          T hi = std::numeric_limits<T>::max()) {
        return get<T>(key, std::nullopt, lo, hi);
    }
    template <class T>
    T opt(std::string_view key, T fallback, T lo = std::numeric_limits<T>::lowest(),
          T hi = std::numeric_limits<T>::max()) {
        return get<T>(key, std::move(fallback), lo, hi);
    }

    // Array of strings; missing is an error only when `required`.
    std::vector<std::string> strings(std::string_view key, bool required) {
        std::vector<std::string> out;
        const Json* v = raw(key);
        if (!v) {
            if (required && json_.is_object())
                fail(std::format("missing required key '{}{}'", prefix_, key));
            return out;
        }
        if (!v->is_array()) {
            fail(std::format("key '{}{}' must be an array of strings", prefix_, key));
            return out;
        }
        for (const Json& s : *v) {
            std::string str;
            if (convert(s, str))
                out.push_back(std::move(str));
            else
                fail(std::format("key '{}{}' must contain only strings", prefix_, key));
        }
        return out;
    }

  private:
    template <class T> T get(std::string_view key, std::optional<T> fallback, T lo, T hi) {
        const Json* v = raw(key);
        if (!v) {
            if (!fallback && json_.is_object())
                fail(std::format("missing required key '{}{}'", prefix_, key));
            return fallback.value_or(T{});
        }
        T out{};
        if (!convert(*v, out)) {
            fail(std::format("key '{}{}' has the wrong type", prefix_, key));
            return fallback.value_or(T{});
        }
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (out < lo || out > hi) {
                fail(hi == std::numeric_limits<T>::max()
                         ? std::format("key '{}{}' must be at least {}", prefix_, key, lo)
                         : std::format("key '{}{}' must be between {} and {}", prefix_, key, lo,
                                       hi));
                return fallback.value_or(T{});
            }
        }
        return out;
    }

    Ctx& ctx_;
    const Json& json_;
    std::string entry_;
    std::string prefix_;
};

template <class E, std::size_t N>
E readEnum(Fields& f, std::string_view key,
           const std::array<std::pair<std::string_view, E>, N>& table) {
    const std::string name = f.req<std::string>(key);
    for (const auto& [text, value] : table)
        if (text == name)
            return value;
    if (f.raw(key) != nullptr)
        f.fail(std::format("unknown {} '{}'", key, name));
    return table[0].second;
}

} // namespace heroes::game::detail
