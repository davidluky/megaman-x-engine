#pragma once

// OID 0x13 law: oid_0x50_pending/attack_law.json.  Direct translation of 83:A652/A65C/A67D,
// 82:80B4, 82:823E, 82:83A3, and the child portion of 87:D492.
// Records are literal 0x40-byte $7E:1428 pool entries; callers seed them once
// from one raw capture.  No per-frame oracle state enters this controller.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mmx::deck_turret_shot {

using Record = std::array<std::uint8_t, 0x40>;
using Pool = std::array<Record, 8>;

inline std::uint16_t read16(const Record& r, std::size_t at) {
  return static_cast<std::uint16_t>(r[at] | (static_cast<std::uint16_t>(r[at + 1]) << 8));
}
inline void write16(Record& r, std::size_t at, std::uint16_t value) {
  r[at] = static_cast<std::uint8_t>(value);
  r[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

struct ParentAllocation {
  std::uint16_t x = 0;       // parent D+05/+06 at 87:D492 entry
  std::uint16_t y = 0;       // parent D+08/+09 at 87:D492 entry
  std::uint8_t selector16 = 0;
  std::uint8_t attr11 = 0;
  std::uint8_t parent18 = 0;
  std::uint8_t parent34 = 0;
};

struct FrameEnvironment {
  bool contact_nonzero = false; // authenticated return of 84:9B03
  std::uint16_t camera_x = 0;
  std::uint16_t camera_y = 0;
};

struct StepResult {
  bool effect = false;      // only 84:A4B5 branch, not a collision verdict
  bool cleared = false;     // 82:83A3 ran
  bool culled = false;      // 82:80B4 wrote $0E=0
};

class Child13Pool {
 public:
  explicit Child13Pool(Pool initial = {}) : pool_(initial) {}
  const Pool& pool() const { return pool_; }
  Pool& pool() { return pool_; }

  // Exact 82:8358 allocator eligibility used by 87:D492: active word $00 == 0.
  std::optional<std::size_t> first_free_slot() const {
    for (std::size_t i = 0; i < pool_.size(); ++i) {
      if (read16(pool_[i], 0x00) == 0) return i;
    }
    return std::nullopt;
  }

  // 87:D492. It deliberately does not assign $04 or $07: their fractions are
  // residue from the same physical pool record and must survive slot reuse.
  std::optional<std::size_t> allocate_from_parent(const ParentAllocation& parent) {
    const auto slot = first_free_slot();
    if (!slot) return std::nullopt;
    Record& r = pool_[*slot];
    write16(r, 0x00, static_cast<std::uint16_t>(read16(r, 0x00) + 1)); // source INC absolute-indexed; eligible word was zero
    r[0x0A] = 0x13;
    r[0x18] = parent.parent18;
    r[0x11] = static_cast<std::uint8_t>(parent.parent34 | parent.attr11);
    r[0x16] = parent.selector16;
    r[0x0B] = 0x02;
    // 87:D4B8 is in 8-bit M mode. STZ.W supplies a 16-bit address operand,
    // but stores one byte: +29 is residue, not part of this initialization.
    r[0x28] = 0;
    const bool right = (parent.attr11 & 0x40u) != 0;
    r[0x1A] = 0x00;
    r[0x1B] = right ? 0x02 : 0xFE;
    write16(r, 0x05, static_cast<std::uint16_t>(parent.x + (right ? 0x20 : -0x20)));
    write16(r, 0x08, static_cast<std::uint16_t>(parent.y - 2));
    write16(r, 0x20, 0xD3ED);
    return slot;
  }

  // One source pool-dispatch call.  State 0 initializes; state 2 performs the
  // live law. Other states are outside this narrow OID 0x13 source slice.
  StepResult dispatch(std::size_t slot, const FrameEnvironment& env) {
    Record& r = pool_.at(slot);
    if (r[0x00] == 0 || r[0x0A] != 0x13) return {};
    // 80:D4D3 runs before the child wrapper: publish object X/Y for the
    // generic collision/render paths and TRB #$80,$0E (keep lower flag bits).
    write16(r, 0x22, read16(r, 0x05));
    write16(r, 0x24, read16(r, 0x08));
    r[0x0E] = static_cast<std::uint8_t>(r[0x0E] & 0x7Fu);
    if (r[0x01] == 0) {
      init(r, env);
      return {};
    }
    if (r[0x01] != 0x02) return {};
    return live(r, env);
  }

 private:
  Pool pool_;

  static void camera_flag(Record& r, const FrameEnvironment& env) {
    const auto x = static_cast<std::uint16_t>(read16(r, 0x05) - env.camera_x + 0x20);
    const auto y = static_cast<std::uint16_t>(read16(r, 0x08) - env.camera_y + 0x10);
    r[0x0E] = (x < 0x0140 && y < 0x0100) ? 0x81 : 0x00;
  }

  static void init(Record& r, const FrameEnvironment& env) {
    r[0x01] = 0x02;
    r[0x27] = 0x02;
    r[0x28] = 0x01;
    r[0x26] = 0x02;
    r[0x12] = 0x02;
    camera_flag(r, env);             // 82:80B4 before timer/anim initialization
    r[0x39] = 0x60;
    // 84:8F07 uses the selector already copied to $16. AFB76E starts here.
    r[0x13] = 0x08;
    write16(r, 0x14, 0xB76E);
    r[0x0F] = 0x00;
    r[0x17] = 0x86;
  }

  static void move_x(Record& r) {
    const std::uint32_t sum = static_cast<std::uint32_t>(read16(r, 0x04)) + read16(r, 0x1A);
    write16(r, 0x04, static_cast<std::uint16_t>(sum));
    const std::uint8_t carry = sum > 0xFFFFu ? 1u : 0u;
    const std::uint8_t high_adjust = (r[0x1B] & 0x80u) ? 0xFFu : 0x00u;
    r[0x06] = static_cast<std::uint8_t>(r[0x06] + high_adjust + carry);
  }

  static void tick_art(Record& r) {
    // 84:8EEA; valid source stream is AFB76E: 86,87,88,89 each duration 8,
    // then F4 FF loops to the first token. DEC of zero wraps, as on 65816.
    r[0x13] = static_cast<std::uint8_t>(r[0x13] - 1);
    if (r[0x13] != 0) return;
    const std::uint16_t ptr = read16(r, 0x14);
    const std::uint16_t next = (ptr == 0xB777) ? 0xB76E : static_cast<std::uint16_t>(ptr + 3);
    write16(r, 0x14, next);
    r[0x13] = 0x08;
    // Stream tokens are [duration, wait/attribute byte, art index].
    r[0x0F] = next == 0xB777 ? 0x80 : 0x00;
    r[0x17] = static_cast<std::uint8_t>(0x86 + ((next - 0xB76E) / 3));
  }

  static void clear_8283a3(Record& r) {
    // 82:83A3: REP #$20; STZ $00,$02,$0E,$2C; SEP #$20.
    write16(r, 0x00, 0);
    write16(r, 0x02, 0);
    write16(r, 0x0E, 0);
    write16(r, 0x2C, 0);
  }

  static StepResult live(Record& r, const FrameEnvironment& env) {
    StepResult out;
    r[0x39] = static_cast<std::uint8_t>(r[0x39] - 1); // DEC $39
    const bool expired = r[0x39] == 0;
    if (expired || env.contact_nonzero) {
      out.effect = r[0x16] == 0x28; // 84:A4B5 only for selector $28
      clear_8283a3(r);
      out.cleared = true;
      return out;
    }
    // $28 is nonzero after init, so the 84:9B43 buster path is skipped.
    move_x(r);
    tick_art(r);
    camera_flag(r, env);
    if (r[0x0E] == 0) {
      clear_8283a3(r);
      out.cleared = true;
      out.culled = true;
    }
    return out;
  }
};

} // namespace mmx::deck_turret_shot
