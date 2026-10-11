#pragma once

#include "engine/input.hpp"
#include "engine/loop.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace heroes::platform {

// RAII: the constructor enters raw mode and the alternate screen, the destructor restores both.
// Only one may exist at a time. The constructor throws std::runtime_error if the process is
// not attached to an interactive terminal. Ctrl+C and termination signals also restore it.
class Terminal final : public engine::LoopIo {
  public:
    Terminal();
    ~Terminal() override;
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    std::pair<int, int> size() const override; // cols, rows
    std::string readAvailable();               // non-blocking raw bytes (POSIX only)
    std::vector<engine::KeyEvent> readKeys() override;
    void write(std::string_view bytes) override;

  private:
#ifndef _WIN32
    engine::InputDecoder decoder_; // Windows decodes key events natively
#endif
};

} // namespace heroes::platform
