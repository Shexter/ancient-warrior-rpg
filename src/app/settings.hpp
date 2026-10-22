#pragma once

#include "game/narration/templates.hpp"
#include "net/narrator.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace heroes::app {

// What the narrator may use (reference/llm-interfaces.md §4). Kept in settings.json beside the
// save, never in the save itself.
struct Settings {
    enum class Mode { Off, Hosted, Byok, Local };
    bool consentShown = false; // the player has seen what each mode sends where
    Mode mode = Mode::Off;
    std::string provider = "anthropic"; // gemini, anthropic or openai (Own key only)
    std::string model, baseUrl, apiKey, installId;
    bool operator==(const Settings&) const = default;
};

// The first-run text explaining each mode, shown before the narrator may send anything.
extern const std::string_view kConsentText;

// The settings in `file`; defaults (consent not yet shown, Off) when it's missing or unreadable.
Settings loadSettings(const std::filesystem::path& file);
// Writes `file` through a temporary file, readable only by the owner on POSIX, then renames it
// over the old one. False when it couldn't.
bool saveSettings(const Settings& settings, const std::filesystem::path& file);

// "••••" and the key's last 4 characters, or just "••••" for a key shorter than 8.
std::string mask(std::string_view key);
// The key to use: the provider's environment variable (GEMINI_API_KEY, ANTHROPIC_API_KEY or
// OPENAI_API_KEY) when set, else the one in the settings. `getEnv` is the lookup to use.
std::string resolveKey(const Settings& settings,
                       const std::function<std::optional<std::string>(const char*)>& getEnv);
// Whether `url` is http(s)://host[:port]/path, with plain http only for localhost, 127.0.0.1
// or ::1.
bool validBaseUrl(std::string_view url);
// 32 lowercase hex characters from the system's random source.
std::string newInstallId();

// Whether this build has a relay for Hosted mode (HEROES_RELAY_URL set at configure time).
bool hostedAvailable();
// Why the settings can't narrate yet ("Set a model name ..."), or empty when they can (or are Off).
std::string settingsProblem(const Settings& settings,
                            const std::function<std::optional<std::string>(const char*)>& getEnv);

// The narrator the settings ask for, sending through `transport`; none before consent, when Off,
// when the mode lacks what it needs (settingsProblem), or for Hosted (no relay client yet). No
// request can leave the game without one.
std::shared_ptr<net::Narrator> makeNarrator(const Settings& settings, const game::narration::NarratorDef& def,
                                            const std::function<std::optional<std::string>(const char*)>& getEnv,
                                            net::Transport transport);

} // namespace heroes::app
