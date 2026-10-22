#include "app/settings.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <format>
#include <fstream>
#include <random>
#include <sstream>
#include <utility>

namespace heroes::app {

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::array<std::pair<std::string_view, Settings::Mode>, 4> kModes{{
    {"off", Settings::Mode::Off},
    {"hosted", Settings::Mode::Hosted},
    {"byok", Settings::Mode::Byok},
    {"local", Settings::Mode::Local},
}};

std::string_view modeName(Settings::Mode mode) {
    for (const auto& [name, m] : kModes)
        if (m == mode)
            return name;
    return "off";
}

} // namespace

const std::string_view kConsentText =
    "AI NARRATOR (optional)\n"
    "The game can describe battles and voice townsfolk using an AI model.\n"
    "  Off      – nothing leaves your computer. Built-in text is used.\n"
    "  Hosted   – free, rate-limited. Battle summaries, your hero's name and what you type to NPCs are sent to\n"
    "             the game's relay server and to Google's Gemini API. Nothing is stored by the relay.\n"
    "  Own key  – the same data goes directly to the provider you choose (Gemini, Anthropic, OpenAI-compatible),\n"
    "             billed to your key.\n"
    "  Local    – the same data goes to a model server on this computer (e.g. Ollama). Nothing leaves your machine.\n"
    "You can change this any time in Settings.";

Settings loadSettings(const fs::path& file) {
    Settings s;
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return s;
    std::ostringstream text;
    text << in.rdbuf();
    try {
        const Json j = Json::parse(text.str());
        s.consentShown = j.value("consentShown", false);
        const Json& n = j.at("narrator");
        const std::string mode = n.value("mode", "off");
        for (const auto& [name, m] : kModes)
            if (name == mode)
                s.mode = m;
        const std::string provider = n.value("provider", "anthropic");
        if (provider == "gemini" || provider == "anthropic" || provider == "openai")
            s.provider = provider;
        s.model = n.value("model", "");
        s.baseUrl = n.value("baseUrl", "");
        s.apiKey = n.value("apiKey", "");
        s.installId = n.value("installId", "");
    } catch (const Json::exception&) {
        return Settings{}; // a damaged file is as good as none
    }
    return s;
}

bool saveSettings(const Settings& s, const fs::path& file) {
    const Json j{{"version", 1},
                 {"consentShown", s.consentShown},
                 {"narrator",
                  {{"mode", modeName(s.mode)},
                   {"provider", s.provider},
                   {"model", s.model},
                   {"baseUrl", s.baseUrl},
                   {"apiKey", s.apiKey},
                   {"installId", s.installId}}}};
    std::error_code ec;
    if (file.has_parent_path())
        fs::create_directories(file.parent_path(), ec);
    fs::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << j.dump(2);
        if (!out.flush()) {
            fs::remove(tmp, ec);
            return false;
        }
    }
#ifndef _WIN32
    fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
#endif
    fs::rename(tmp, file, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

std::string mask(std::string_view key) {
    if (key.size() < 8)
        return "••••";
    return "••••" + std::string(key.substr(key.size() - 4));
}

std::string resolveKey(const Settings& s,
                       const std::function<std::optional<std::string>(const char*)>& getEnv) {
    const char* var = s.provider == "gemini" ? "GEMINI_API_KEY"
                      : s.provider == "openai" ? "OPENAI_API_KEY"
                                               : "ANTHROPIC_API_KEY";
    if (auto fromEnv = getEnv(var); fromEnv && !fromEnv->empty())
        return *fromEnv;
    return s.apiKey;
}

bool validBaseUrl(std::string_view url) {
    const bool https = url.starts_with("https://");
    if (!https && !url.starts_with("http://"))
        return false;
    std::string_view rest = url.substr(https ? 8 : 7);
    const std::size_t slash = rest.find('/');
    if (slash == std::string_view::npos || slash == 0)
        return false; // needs a host and a path
    std::string_view hostPort = rest.substr(0, slash);
    std::string_view host = hostPort;
    if (hostPort.starts_with('[')) { // [::1]:port
        const std::size_t close = hostPort.find(']');
        if (close == std::string_view::npos)
            return false;
        host = hostPort.substr(1, close - 1);
        hostPort = hostPort.substr(close + 1);
        if (!hostPort.empty() && !hostPort.starts_with(':'))
            return false;
    } else if (const std::size_t colon = hostPort.find(':'); colon != std::string_view::npos) {
        host = hostPort.substr(0, colon);
        hostPort = hostPort.substr(colon);
    } else {
        hostPort = {};
    }
    if (host.empty())
        return false;
    if (hostPort.starts_with(':')) {
        const std::string_view port = hostPort.substr(1);
        if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string_view::npos)
            return false;
    }
    return https || host == "localhost" || host == "127.0.0.1" || host == "::1";
}

bool hostedAvailable() { return std::string_view(HEROES_RELAY_URL).size() > 0; }

std::string settingsProblem(const Settings& s,
                            const std::function<std::optional<std::string>(const char*)>& getEnv) {
    switch (s.mode) {
    case Settings::Mode::Off:
        return {};
    case Settings::Mode::Hosted:
        return hostedAvailable() ? "" : "Not available in this build.";
    case Settings::Mode::Byok:
        return resolveKey(s, getEnv).empty() ? "Enter an API key (or set the provider's environment variable)." : "";
    case Settings::Mode::Local:
        if (!s.baseUrl.empty() && !validBaseUrl(s.baseUrl))
            return "The base URL must be http(s)://host[:port]/path, with plain http only on this machine.";
        return s.model.empty() ? "Set a model name (the one you pulled, e.g. with `ollama pull`)." : "";
    }
    return {};
}

std::shared_ptr<net::Narrator> makeNarrator(const Settings& s, const game::narration::NarratorDef& def,
                                            const std::function<std::optional<std::string>(const char*)>& getEnv,
                                            net::Transport transport) {
    if (!s.consentShown || s.mode == Settings::Mode::Off || !settingsProblem(s, getEnv).empty())
        return nullptr;
    if (s.mode == Settings::Mode::Hosted) // only the event and the install id leave the game
        return std::make_shared<net::Narrator>(net::relayProvider(def, HEROES_RELAY_URL, s.installId), std::move(transport),
                                               net::Narrator::Options{def.limits.narrationChars, false});
    const net::ProviderConfig config{s.model, resolveKey(s, getEnv), s.baseUrl};
    std::unique_ptr<net::Provider> provider;
    if (s.mode == Settings::Mode::Local)
        provider = net::openAiProvider(def, config, true);
    else if (s.provider == "gemini")
        provider = net::geminiProvider(def, config);
    else if (s.provider == "openai")
        provider = net::openAiProvider(def, config, false);
    else
        provider = net::anthropicProvider(def, config);
    return std::make_shared<net::Narrator>(std::move(provider), std::move(transport),
                                           net::Narrator::Options{def.limits.narrationChars, s.mode == Settings::Mode::Local});
}

std::string newInstallId() {
    std::random_device device;
    const auto draw = [&] { return (static_cast<std::uint64_t>(device()) << 32) | device(); };
    return std::format("{:016x}{:016x}", draw(), draw());
}

} // namespace heroes::app
