#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <memory>
#include <optional>

namespace heroes::app {

// Saves the session's settings and rebuilds its narrator from them (none before consent, when Off,
// or without a transport). Returns false when the file couldn't be written.
bool applySettings(Session& session);

// The first screen: the consent screen when the settings file has never recorded the player's
// choice, else the Title.
std::unique_ptr<Screen> firstScreen(Session& session);

// First run: what each narrator mode sends where, and the choice of one. Nothing is sent before
// a mode other than Off is chosen; Own key and Local are then set up under Settings.
class ConsentScreen final : public Screen {
  public:
    explicit ConsentScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    Menu menu_;
};

// The narrator's settings from the Hub: mode, provider, model, base URL and API key (shown
// masked). Every change is saved and the narrator rebuilt at once.
class SettingsScreen final : public Screen {
  public:
    explicit SettingsScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    enum class Field { Model, BaseUrl, ApiKey };
    std::vector<Menu::Item> rows() const;

    Session& session_;
    Menu menu_;
    std::optional<std::pair<Field, TextInput>> editing_;
    std::string note_; // what the last change did
};

} // namespace heroes::app
