#pragma once

#include "window/scene.hpp"

#include <array>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>

struct ModelAnimation; // raylib

namespace heroes::window {

// The four animations a character plays, and the KayKit clip each one is (D8).
enum class Clip { Idle, Walk, Attack, Hit };
inline constexpr std::array<const char*, 4> kClipNames{"Idle", "Walking_A", "1H_Melee_Attack_Chop",
                                                       "Hit_A"};

// Each clip's length in seconds (the same in every KayKit character).
inline constexpr std::array<float, 4> kClipSeconds{1.067f, 1.067f, 1.067f, 0.667f};
// Walk clip seconds per tile walked, so the feet keep pace with the ground at any speed.
inline constexpr float kWalkSecondsPerTile = 0.45f;
inline constexpr float kCrossfadeSeconds = 0.15f;

// What a model shows this frame: `clip` at `time` seconds in, faded in over `from` at `fromTime`
// by `blend` (1 = `clip` alone), turned `yaw` radians from facing down the screen (positive turns
// toward screen right).
struct Pose {
    Clip clip = Clip::Idle;
    float time = 0;
    Clip from = Clip::Idle;
    float fromTime = 0;
    float blend = 1;
    float yaw = 0;
};

// What a creature did since the last frame.
struct Doing {
    Scene::Point moved{0, 0}; // in tiles; none when it stood still
    bool attacked = false;    // an attack started
    bool hit = false;         // it was hit
};

// The pose `dt` seconds after `previous`. The yaw eases toward the way it moves, most of the way
// in 0.12 s, the short way round. Walk advances by the distance moved and idle by time; attack and
// hit play once, then it walks or idles again. A change of clip crossfades over 0.15 s.
Pose poseOf(const Pose& previous, const Doing& doing, float dt);

// A model file whose animations have loaded and have every clip.
struct ModelFile {
    std::filesystem::path file;
    ModelAnimation* anims = nullptr;
    int count = 0;
    std::array<int, 4> clips{}; // index into anims, per Clip
};

// hero_<id>.glb in `dir` for each path id. A missing file is left out (the hero keeps the battle
// sprite); a file that doesn't load, or lacks a clip, is an error naming it. Needs no window.
// Sets raylib's log level to errors only.
std::expected<std::map<std::string, ModelFile>, std::string>
loadModelFiles(const std::filesystem::path& dir, std::span<const std::string> pathIds);

// Draws a model into a 48x64 off-screen slot and reads it back. Needs the window open, and must
// not be called between BeginTextureMode and EndTextureMode.
class ModelPainter {
  public:
    // Takes the animations. Throws std::runtime_error naming a file whose meshes don't load or
    // aren't skinned (raylib only animates skinned meshes).
    explicit ModelPainter(std::map<std::string, ModelFile> files);
    ~ModelPainter();
    ModelPainter(const ModelPainter&) = delete;
    ModelPainter& operator=(const ModelPainter&) = delete;

    // `id`'s model in `pose`, top row first; null when `id` has no model. Valid until the next call.
    const Pixels* paint(const std::string& id, const Pose& pose);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace heroes::window
