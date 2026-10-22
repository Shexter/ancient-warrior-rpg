#include "app/screens/narrator_setup.hpp"

#include "app/screens/title.hpp"

#include <array>
#include <format>

namespace heroes::app {

namespace {

using Mode = Settings::Mode;
using engine::Key;
using engine::KeyEvent;

std::string_view modeLabel(Mode mode) {
    switch (mode) {
    case Mode::Off:
        return "Off";
    case Mode::Hosted:
        return "Hosted";
    case Mode::Byok:
        return "Own key";
    case Mode::Local:
        return "Local";
    }
    return "Off";
}

enum SettingsRow { kMode, kProvider, kModel, kBaseUrl, kApiKey, kBack };

} // namespace

bool applySettings(Session& session) {
    const bool saved = session.settingsFile.empty() || saveSettings(session.settings, session.settingsFile);
    session.liveNarrator.reset();
    if (session.transport && session.narrator && session.env)
        session.liveNarrator = makeNarrator(session.settings, *session.narrator, session.env, session.transport);
    return saved;
}

std::unique_ptr<Screen> firstScreen(Session& session) {
    if (!session.settingsFile.empty() && !session.settings.consentShown)
        return std::make_unique<ConsentScreen>(session);
    return std::make_unique<TitleScreen>(session);
}

// ---- Consent -----------------------------------------------------------------------------

ConsentScreen::ConsentScreen(Session& session)
    : session_(session),
      menu_({{"Off"},
             {"Hosted", hostedAvailable(), hostedAvailable() ? "" : "Not available in this build"},
             {"Own key"},
             {"Local"}}) {}

Transition ConsentScreen::update(std::span<const KeyEvent> keys, double) {
    static constexpr std::array<Mode, 4> kModes{Mode::Off, Mode::Hosted, Mode::Byok, Mode::Local};
    for (const KeyEvent& key : keys)
        if (const auto chosen = menu_.handle(key)) {
            session_.settings.mode = kModes[static_cast<std::size_t>(*chosen)];
            session_.settings.consentShown = true;
            applySettings(session_);
            return Replace{std::make_unique<TitleScreen>(session_)};
        }
    return Stay{};
}

void ConsentScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    int y = 1;
    std::string_view text = kConsentText;
    bool first = true;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = text.substr(0, end);
        for (const std::string& row : wordWrap(line, frame.width() - 4))
            drawText(frame, 2, y++, row, first ? palette::kAccent : palette::kText);
        first = false;
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    }
    menu_.draw(frame, 4, y + 1);
    drawText(frame, 2, frame.height() - 1, "Enter choose", palette::kDim);
}

// ---- Settings ----------------------------------------------------------------------------

SettingsScreen::SettingsScreen(Session& session) : session_(session), menu_(rows()) {}

std::vector<Menu::Item> SettingsScreen::rows() const {
    const Settings& s = session_.settings;
    const bool byok = s.mode == Mode::Byok, local = s.mode == Mode::Local;
    const std::string key = session_.env ? resolveKey(s, session_.env) : s.apiKey;
    const bool fromEnv = !key.empty() && key != s.apiKey;
    return {{std::format("Mode: {}", modeLabel(s.mode))},
            {std::format("Provider: {}", s.provider), byok, byok ? "" : "Own key only"},
            {std::format("Model: {}", s.model.empty() ? "(the default)" : s.model), byok || local},
            {std::format("Base URL: {}", s.baseUrl.empty() ? "(the default)" : s.baseUrl), local, local ? "" : "Local only"},
            {std::format("API key: {}", key.empty() ? "(none)" : mask(key)), byok,
             fromEnv ? "from the environment" : ""},
            {"Back"}};
}

Transition SettingsScreen::update(std::span<const KeyEvent> keys, double dtSeconds) {
    Settings& s = session_.settings;
    if (editing_)
        editing_->second.tick(dtSeconds);
    for (const KeyEvent& key : keys) {
        if (editing_) {
            const InputResult r = editing_->second.handle(key);
            if (r == InputResult::Editing)
                continue;
            if (r == InputResult::Submitted) {
                const std::string value = editing_->second.value();
                if (editing_->first == Field::BaseUrl && !value.empty() && !validBaseUrl(value)) {
                    note_ = "That base URL isn't allowed: http(s)://host[:port]/path, plain http only on this machine.";
                } else {
                    (editing_->first == Field::Model ? s.model : editing_->first == Field::BaseUrl ? s.baseUrl : s.apiKey) = value;
                    note_ = applySettings(session_) ? "Saved." : "Couldn't save the settings.";
                }
            }
            editing_.reset();
            menu_.setItems(rows());
            continue;
        }
        if (key.key == Key::Escape)
            return Pop{};
        const auto chosen = menu_.handle(key);
        if (!chosen)
            continue;
        switch (*chosen) {
        case kMode: { // Off -> (Hosted) -> Own key -> Local -> Off
            const std::array<Mode, 4> order{Mode::Off, Mode::Hosted, Mode::Byok, Mode::Local};
            std::size_t i = 0;
            while (order[i] != s.mode)
                ++i;
            do
                i = (i + 1) % order.size();
            while (order[i] == Mode::Hosted && !hostedAvailable());
            s.mode = order[i];
            if (s.mode == Mode::Hosted && s.installId.empty())
                s.installId = newInstallId();
            note_ = applySettings(session_) ? "Saved." : "Couldn't save the settings.";
            break;
        }
        case kProvider:
            s.provider = s.provider == "anthropic" ? "gemini" : s.provider == "gemini" ? "openai" : "anthropic";
            s.model.clear(); // another provider's model name won't fit
            note_ = applySettings(session_) ? "Saved." : "Couldn't save the settings.";
            break;
        case kModel:
            editing_.emplace(Field::Model, TextInput(100));
            break;
        case kBaseUrl:
            editing_.emplace(Field::BaseUrl, TextInput(200));
            break;
        case kApiKey:
            editing_.emplace(Field::ApiKey, TextInput(200));
            break;
        case kBack:
            return Pop{};
        default:
            break;
        }
        menu_.setItems(rows());
    }
    return Stay{};
}

void SettingsScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    drawText(frame, 2, 1, "SETTINGS · AI NARRATOR", palette::kAccent);
    menu_.draw(frame, 4, 3, !editing_, frame.width() - 6);
    int y = 11;
    if (editing_) {
        const std::string_view label = editing_->first == Field::Model ? "Model: "
                                       : editing_->first == Field::BaseUrl ? "Base URL: "
                                                                           : "API key: ";
        const int x = 4 + drawText(frame, 4, y, label, palette::kText);
        if (editing_->first == Field::ApiKey) // never shown as typed
            drawText(frame, x, y, mask(editing_->second.value()) + "_", palette::kText);
        else
            editing_->second.draw(frame, x, y);
        ++y;
    }
    const std::string problem = settingsProblem(session_.settings, session_.env ? session_.env : [](const char*) {
        return std::optional<std::string>();
    });
    for (const std::string& row : wordWrap(problem, frame.width() - 6))
        drawText(frame, 4, ++y, row, palette::kDmg);
    for (const std::string& row : wordWrap(note_, frame.width() - 6))
        drawText(frame, 4, ++y, row, palette::kDim);
    drawText(frame, 2, frame.height() - 3,
             "Keys are kept in settings.json beside your save, readable only by you, and never in", palette::kDim);
    drawText(frame, 2, frame.height() - 2, "the save. An environment variable (e.g. ANTHROPIC_API_KEY) wins over it.", palette::kDim);
    drawText(frame, 2, frame.height() - 1, editing_ ? "Enter save · Esc cancel" : "Enter change · Esc back", palette::kDim);
}

} // namespace heroes::app
