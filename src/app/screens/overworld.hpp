#pragma once

#include "app/held_keys.hpp"
#include "app/screen.hpp"
#include "game/world/world.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace heroes::app {

// The real-time world map: the hero walks with the arrows or W/A/S/D (held keys keep walking),
// swings with Space or a click, casts Firebolt (1) and Nova (2), drinks a potion with Q, opens a
// landmark's screen with E, shows or hides the minimap with M, and returns to the Hub with Esc.
// A status panel, a goal panel and a hotbar are drawn over the map. Requires a game state and a
// loaded world.
class OverworldScreen final : public Screen {
  public:
    explicit OverworldScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The whole terminal from 80x24 up to 200x60 (80x24 under --layout compact).
    engine::Size canvasSize(engine::Size terminal) const override;
    bool wantsKeyActions() const override { return true; }

    const game::world::World& world() const { return world_; }

  private:
    // This step's input from the keys, or nothing when Esc asks to leave.
    std::optional<game::world::WorldInput> readKeys(std::span<const engine::KeyEvent> keys);
    // Reacts to one world event; a transition when it leaves the screen.
    std::optional<Transition> handle(const game::world::WorldEvent& event);
    void notify(std::string text); // a short message in the goal line
    void ageEffects();
    void drawHotbar(engine::Framebuffer& frame) const;
    // The screen a landmark opens, or nothing (a locked lair only repeats its note).
    std::unique_ptr<Screen> open(const game::world::Landmark& landmark);
    // "Forge — press E", "Lair of Avaritia — Requires Lv 15", or empty away from landmarks.
    std::string landmarkPrompt() const;
    bool minimapShown(int canvasWidth) const;
    void drawTerrain(engine::Framebuffer& frame) const;
    void drawHud(engine::Framebuffer& frame) const;
    void drawMinimap(engine::Framebuffer& frame) const;
    void drawCreatures(engine::Framebuffer& frame) const; // monsters, the hero, damage numbers

    Session& session_;
    game::world::World world_;
    HeldKeys held_;
    // The Hub's "Next:" line, worked out on entry and whenever a landmark's screen closes (a
    // goal costs several 200-fight estimates, so never per frame).
    std::string goal_;
    bool refreshGoal_ = false;
    std::optional<bool> minimap_;      // the player's M choice; unset follows the canvas width
    struct FloatingNumber {            // a hit's damage, rising over where it landed
        game::world::Tile at;
        int amount;
        bool onHero;
        int age; // ticks shown so far
    };
    std::vector<FloatingNumber> numbers_;
    struct NovaRing {
        game::world::Tile center;
        int radius, age;
    };
    std::optional<NovaRing> nova_;
    std::string notice_; // "No potions", a kill's rewards; shown over the goal line for a while
    int noticeAge_ = 0;
    std::array<int, game::world::kAbilityCount + 1> flashes_{}; // hotbar slot flash ticks left
    int ticks_ = 0; // steps since the screen opened, for the wind-up flash
    mutable int lastCanvasWidth_ = 80; // for M before a choice is made
    // The map tile drawn at the canvas's top-left. It follows the hero as frames are drawn, since
    // only drawing knows the canvas size; unset until the first frame centres it on the hero.
    mutable std::optional<game::world::Tile> camera_;
};

} // namespace heroes::app
