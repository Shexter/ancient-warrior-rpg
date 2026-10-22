#include "window/models.hpp"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace heroes::window {

namespace {

constexpr int kSlotW = 48, kSlotH = 64;
constexpr float kFrameSeconds = 0.017f; // raylib samples glTF clips every 17 ms

// A key light from the upper left, a little ambient and a cool rim, so the glyphs read as solid.
constexpr const char* kVertex = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal;
uniform mat4 mvp; uniform mat4 matNormal;
out vec2 uv; out vec3 n;
void main() {
    uv = vertexTexCoord;
    n = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
})";
constexpr const char* kFragment = R"(#version 330
in vec2 uv; in vec3 n;
uniform sampler2D texture0; uniform vec4 colDiffuse;
out vec4 o;
void main() {
    vec4 c = texture(texture0, uv) * colDiffuse;
    float d = max(dot(normalize(n), normalize(vec3(-0.5, 0.9, 0.6))), 0.0);
    float rim = pow(1.0 - max(n.z, 0.0), 3.0) * 0.25;
    o = vec4(pow(c.rgb, vec3(0.8)) * (0.6 + 0.75 * d) + vec3(0.55, 0.65, 0.9) * rim, c.a);
})";

} // namespace

std::expected<std::map<std::string, ModelFile>, std::string>
loadModelFiles(const std::filesystem::path& dir, std::span<const std::string> pathIds) {
    SetTraceLogLevel(LOG_ERROR); // raylib logs a line per clip otherwise; run() keeps this level
    std::map<std::string, ModelFile> files;
    const auto fail = [&](std::string message) {
        for (auto& [id, f] : files)
            UnloadModelAnimations(f.anims, f.count);
        return std::unexpected(std::move(message));
    };
    for (const std::string& id : pathIds) {
        ModelFile f{dir / ("hero_" + id + ".glb")};
        if (std::error_code ec; !std::filesystem::is_regular_file(f.file, ec))
            continue;
        const std::string name = f.file.string();
        // Only a binary glTF goes to raylib: it leaks the file's bytes when it can't parse them.
        char magic[4] = {};
        std::ifstream(f.file, std::ios::binary).read(magic, 4);
        if (std::string_view(magic, 4) != "glTF")
            return fail(name + ": not a loadable animated model");
        f.anims = LoadModelAnimations(name.c_str(), &f.count);
        files.emplace(id, f); // so `fail` unloads it too
        if (!f.anims || f.count == 0)
            return fail(name + ": not a loadable animated model");
        for (std::size_t c = 0; c < kClipNames.size(); ++c) {
            int i = 0;
            while (i < f.count && std::string_view(f.anims[i].name) != kClipNames[c])
                ++i;
            if (i == f.count)
                return fail(name + ": missing clip '" + kClipNames[c] + "'");
            files.at(id).clips[c] = i;
        }
    }
    return files;
}

Pose poseOf(const Pose& previous, const Doing& doing, float dt) {
    Pose p = previous;
    const float distance = std::hypot(doing.moved.x, doing.moved.y);
    const bool moving = distance > 0;
    if (moving) {
        const float turn = std::remainder(std::atan2(doing.moved.x, doing.moved.y) - p.yaw,
                                          2 * std::numbers::pi_v<float>);
        p.yaw = std::remainder(p.yaw + turn * (1 - std::exp(-dt / 0.04f)), 2 * std::numbers::pi_v<float>);
    }
    const bool playingOnce = (p.clip == Clip::Attack || p.clip == Clip::Hit) &&
                             p.time < kClipSeconds[static_cast<std::size_t>(p.clip)];
    const Clip want = doing.hit ? Clip::Hit
                      : doing.attacked ? Clip::Attack
                      : playingOnce   ? p.clip
                      : moving        ? Clip::Walk
                                      : Clip::Idle;
    if (want != p.clip || doing.hit || doing.attacked) {
        p.from = p.clip;
        p.fromTime = p.time;
        p.clip = want;
        p.time = 0;
        p.blend = 0;
    }
    p.time += p.clip == Clip::Walk ? distance * kWalkSecondsPerTile : dt;
    p.blend = std::min(1.0f, p.blend + dt / kCrossfadeSeconds);
    return p;
}

struct ModelPainter::Impl {
    struct Loaded {
        ModelFile file;
        Model model;
        float height; // in model units, standing in the bind pose
        std::vector<Transform> blended{}; // one frame of bones, between two clips
    };
    std::map<std::string, Loaded> models;
    Shader lit{};
    RenderTexture2D slot{};
    Pixels pixels;

    ~Impl() {
        for (auto& [id, m] : models) {
            for (int i = 0; i < m.model.materialCount; ++i) {
                m.model.materials[i].shader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
                const Texture2D t = m.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
                if (t.id != rlGetTextureIdDefault())
                    UnloadTexture(t);
            }
            UnloadModel(m.model);
            UnloadModelAnimations(m.file.anims, m.file.count);
        }
        if (slot.id)
            UnloadRenderTexture(slot);
        if (lit.id)
            UnloadShader(lit);
    }
};

ModelPainter::ModelPainter(std::map<std::string, ModelFile> files) : impl_(std::make_unique<Impl>()) {
    impl_->lit = LoadShaderFromMemory(kVertex, kFragment);
    impl_->slot = LoadRenderTexture(kSlotW, kSlotH);
    for (auto& [id, f] : files) {
        Model model = LoadModel(f.file.string().c_str());
        bool skinned = model.meshCount > 0;
        for (int i = 0; i < model.meshCount; ++i)
            skinned = skinned && model.meshes[i].boneIds && model.meshes[i].boneWeights;
        for (int i = 0; i < model.materialCount; ++i)
            model.materials[i].shader = impl_->lit;
        const BoundingBox box = GetModelBoundingBox(model);
        impl_->models.emplace(id, Impl::Loaded{f, model, box.max.y - box.min.y}); // Impl unloads it
        if (!skinned) // ponytail: the files not yet reached leak their animations; the game exits
            throw std::runtime_error(f.file.string() + ": every mesh must be skinned to the armature");
    }
}

ModelPainter::~ModelPainter() = default;

const Pixels* ModelPainter::paint(const std::string& id, const Pose& pose) {
    const auto it = impl_->models.find(id);
    if (it == impl_->models.end())
        return nullptr;
    Impl::Loaded& m = it->second;
    const auto frameOf = [&](Clip c, float time) -> const Transform* {
        const ModelAnimation& a = m.file.anims[m.file.clips[static_cast<std::size_t>(c)]];
        return a.framePoses[static_cast<int>(time / kFrameSeconds) % std::max(1, a.frameCount)];
    };
    // raylib 5.5 can't blend clips, so the two frames' bones are mixed here and skinned as a
    // one-frame clip.
    const ModelAnimation& clip = m.file.anims[m.file.clips[static_cast<std::size_t>(pose.clip)]];
    const Transform* to = frameOf(pose.clip, pose.time);
    const Transform* from = frameOf(pose.from, pose.fromTime);
    m.blended.resize(static_cast<std::size_t>(clip.boneCount));
    for (int b = 0; b < clip.boneCount; ++b)
        m.blended[static_cast<std::size_t>(b)] = {
            Vector3Lerp(from[b].translation, to[b].translation, pose.blend),
            QuaternionSlerp(from[b].rotation, to[b].rotation, pose.blend),
            Vector3Lerp(from[b].scale, to[b].scale, pose.blend)};
    Transform* frames[] = {m.blended.data()};
    ModelAnimation mixed = clip;
    mixed.frameCount = 1;
    mixed.framePoses = frames;
    UpdateModelAnimation(m.model, mixed, 0);
    m.model.transform = MatrixRotateY(pose.yaw);

    // A fixed three-quarter top-down orthographic camera, pitched 40 degrees.
    const float pitch = 40 * DEG2RAD, mid = m.height * 0.5f;
    Camera3D camera{};
    camera.target = {0, mid, 0};
    camera.position = {0, mid + std::sin(pitch) * 10, std::cos(pitch) * 10};
    camera.up = {0, 1, 0};
    camera.fovy = m.height * 1.75f;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(impl_->slot);
    ClearBackground({0, 0, 0, 0});
    BeginMode3D(camera);
    DrawModel(m.model, {0, 0, 0}, 1, WHITE);
    EndMode3D();
    EndTextureMode();

    // ponytail: one readback per model per frame; batch creatures into one texture if many get models.
    Image image = LoadImageFromTexture(impl_->slot.texture);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const auto* px = static_cast<const unsigned char*>(image.data);
    Pixels& out = impl_->pixels;
    out.w = kSlotW;
    out.h = kSlotH;
    out.px.resize(static_cast<std::size_t>(kSlotW * kSlotH));
    for (int y = 0; y < kSlotH; ++y) // GL rows are bottom first
        for (int x = 0; x < kSlotW; ++x) {
            const unsigned char* p = px + 4 * ((kSlotH - 1 - y) * kSlotW + x);
            out.px[static_cast<std::size_t>(y * kSlotW + x)] = {p[0], p[1], p[2], p[3]};
        }
    UnloadImage(image);
    return &out;
}

} // namespace heroes::window
