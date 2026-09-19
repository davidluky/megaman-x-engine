#pragma once
// Source: oid_0x50_pending/attack_law.json; 87:D3AF/D405/D43D/D472.
// Controlled ghosts seed source fields once; fresh actors run state00 initialization.

#include <array>
#include <cstddef>
#include <cstdint>

namespace mmx::deck_turret_parent {

// External input is deliberately supplied per step. The controller never
// receives an expected per-frame state from the capture.
struct PlayerInput {
  int player_x = 0;
  bool player_bd3 = false;
  std::uint8_t helper_angle = 0; // 84:A081 result when terrain byte BD3 is nonzero.
};

struct ParentFields {
  std::uint8_t state = 0x00;
  std::uint8_t wait_count = 0;
  std::uint8_t timer_33 = 0;
  std::uint8_t flag_35 = 0;
  std::uint8_t flag_36 = 0;
  std::uint8_t selector_16 = 0x28;
  std::uint8_t art_17 = 0x80;
  std::uint8_t anim_remaining_13 = 0;
  std::uint8_t attr_11 = 0x3F;
  std::uint16_t sprite_ptr_14 = 0;
  int parent_x = 0;
};

struct StepResult {
  bool fired = false;
  bool entered_state4 = false;
};

// Exact 84:A081 / 86:BB5A quantization, before any child-specific angle remap.
constexpr std::uint8_t aim(std::uint16_t x, std::uint16_t y,
                           std::uint16_t playerX, std::uint16_t playerY) {
  std::uint16_t minor = static_cast<std::uint16_t>(playerX - x);
  std::uint16_t major = static_cast<std::uint16_t>(y - playerY);
  std::uint8_t index = 0;
  if (minor & 0x8000) { minor = static_cast<std::uint16_t>(0u - minor); index += 0x20; }
  if (major & 0x8000) { major = static_cast<std::uint16_t>(0u - major); index += 0x10; }
  if (major < minor) { const auto old = major; major = minor; minor = old; index += 8; }
  minor = static_cast<std::uint16_t>(minor << 3);
  const auto increment = static_cast<std::uint16_t>(major << 1);
  for (int left = 4; left && major < minor; --left) {
    major = static_cast<std::uint16_t>(major + increment); ++index;
  }
  constexpr std::array<std::uint8_t,64> table{{
    0,1,2,3,4,0,0,0,8,7,6,5,4,0,0,0,16,15,14,13,12,0,0,0,8,9,10,11,12,0,0,0,
    0,31,30,29,28,0,0,0,24,25,26,27,28,0,0,0,16,17,18,19,20,0,0,0,24,23,22,21,20,0,0,0}};
  return table[index & 0x3F];
}

// Source-derived OID50 parent state machine. D+14/D+15 selects the AF-bank
// record, while D+13 is the live remaining count and must be seeded from the
// source row; pointer phase alone is insufficient for a mid-record seed.
class ParentController {
 public:
  explicit ParentController(ParentFields fields) : fields_(fields) {
    select_animation_stream_from_pointer();
  }

  const ParentFields& fields() const { return fields_; }

  StepResult tick(const PlayerInput& input) {
    StepResult result;
    if (fields_.state == 0x00) {
      // 87:D405 loads sprite/health, orients via 87:9ED4, then timer1/stream1.
      fields_.state = 0x02;
      fields_.flag_35 = 0;
      fields_.timer_33 = 1;
      fields_.attr_11 = static_cast<std::uint8_t>((fields_.attr_11 & 0xBF) |
          (static_cast<std::uint16_t>(input.player_x) >= static_cast<std::uint16_t>(fields_.parent_x) ? 0x40 : 0));
      set_animation1();
    } else if (fields_.state == 0x02) {
      tick_state2(result);
    } else if (fields_.state == 0x04) {
      tick_state4();
    }

    // Authentic main handler calls CODE_FN_87D53A only when the wait mask and
    // state permit it. This gate is independent from the normal state-2 burst.
    if ((fields_.wait_count & 0x06) != 0x06 && fields_.state != 0x04) {
      facing_gate(input, result);
    }
    return result;
  }

 private:
  struct AnimationRecord {
    std::uint16_t pointer;
    std::uint8_t remaining;
    std::uint8_t wait;
    std::uint8_t art;
  };

  // Exact AF-bank records for selector $28, witnessed at AF:B74F and
  // AF:B75D. updateSpriteAnim decrements D+13 and advances by 3 bytes only
  // when it reaches zero (84:8EEA); setSpriteAnim loads the first record
  // (84:8F07). The terminal wait values are source data, not fit constants.
  static constexpr std::array<AnimationRecord, 4> kBurst{{
      {0xB74F, 0x06, 0x06, 0x00},
      {0xB752, 0x06, 0x06, 0x01},
      {0xB755, 0x06, 0x06, 0x02},
      {0xB758, 0x06, 0x86, 0x01},
  }};
  static constexpr std::array<AnimationRecord, 5> kStream1{{
      {0xB75D, 0x08, 0x00, 0x00},
      {0xB760, 0x08, 0x00, 0x03},
      {0xB763, 0x08, 0x00, 0x04},
      {0xB766, 0x08, 0x00, 0x05},
      {0xB769, 0x08, 0x80, 0x05},
  }};

  ParentFields fields_;
  int animation_stream_ = -1; // 0 burst, 1 stream selected by setSpriteAnim(1).
  std::size_t animation_index_ = 0;

  static bool nonnegative(std::uint8_t value) { return (value & 0x80) == 0; }

  void select_animation_stream_from_pointer() {
    animation_stream_ = -1;
    for (std::size_t i = 0; i < kBurst.size(); ++i) {
      if (kBurst[i].pointer == fields_.sprite_ptr_14) {
        animation_stream_ = 0;
        animation_index_ = i;
        return;
      }
    }
    for (std::size_t i = 0; i < kStream1.size(); ++i) {
      if (kStream1[i].pointer == fields_.sprite_ptr_14) {
        animation_stream_ = 1;
        animation_index_ = i;
        return;
      }
    }
  }

  template <std::size_t N>
  void load_record(const std::array<AnimationRecord, N>& stream, std::size_t index) {
    const auto& record = stream[index];
    fields_.sprite_ptr_14 = record.pointer;
    fields_.anim_remaining_13 = record.remaining;
    fields_.wait_count = record.wait;
    fields_.art_17 = static_cast<std::uint8_t>(record.art | 0x80); // 84:8F4D TSB $17
    animation_index_ = index;
  }

  void set_animation0() {
    animation_stream_ = 0;
    load_record(kBurst, 0);
  }

  void set_animation1() {
    animation_stream_ = 1;
    load_record(kStream1, 0);
  }

  void advance_animation() {
    if (animation_stream_ < 0) return;
    if (--fields_.anim_remaining_13 != 0) return;
    if (animation_stream_ == 0) {
      load_record(kBurst, (animation_index_ + 1) % kBurst.size()); // AF:B75B F4 FF
    } else {
      load_record(kStream1, animation_index_ + 1 < kStream1.size() ? animation_index_ + 1 : animation_index_); // AF:B76C FD FF
    }
  }

  void tick_state2(StepResult& result) {
    fields_.timer_33 = static_cast<std::uint8_t>(fields_.timer_33 - 1);
    if (fields_.timer_33 != 0) return;

    if (fields_.flag_35 == 0) {
      set_animation0();
      fields_.flag_35 = 1;
    }

    if (nonnegative(fields_.wait_count)) {
      ++fields_.timer_33;
      advance_animation();
      return;
    }

    // CODE_FN_87D492 succeeds in the controlled capture. Allocation details
    // belong to the child-pool model; this parent controller emits the edge.
    result.fired = true;
    fields_.timer_33 = 0x5A;
    set_animation1();
    fields_.flag_35 = 0;
  }

  void tick_state4() {
    if (nonnegative(fields_.wait_count)) {
      advance_animation();
      return;
    }
    fields_.attr_11 ^= 0x40;
    set_animation1();
    fields_.state = 0x02;
    fields_.timer_33 = 0x5A;
  }

  void enter_state4(StepResult& result) {
    fields_.state = 0x04;
    set_animation1();
    result.entered_state4 = true;
  }

  void facing_gate(const PlayerInput& input, StepResult& result) {
    const bool rightSide = static_cast<std::uint16_t>(input.player_x) >= static_cast<std::uint16_t>(fields_.parent_x);
    const bool flip = (fields_.attr_11 & 0x40) != 0;
    // This is the branch polarity in 87:D53A: dx>=0 with bit40 clear, or
    // dx<0 with bit40 set, reaches the state-4 transition directly.
    const bool direct_transition = (rightSide && !flip) || (!rightSide && flip);
    if (direct_transition) {
      enter_state4(result);
      return;
    }
    if (!input.player_bd3) return;

    // CODE_FN_87D576 suppresses the transition when its helper result is in
    // [4,1C). Outside that range it takes the same state-4 path.
    const auto below = static_cast<std::uint8_t>(input.helper_angle - 4);
    const auto above = static_cast<std::uint8_t>(input.helper_angle - 0x1C);
    if (!(below & 0x80) && (above & 0x80)) {
      set_animation1(); // 87:D584 resets stream1 even when the turn is suppressed.
      fields_.flag_36 = 1;
      return;
    }
    fields_.flag_36 = 0;
    enter_state4(result);
  }
};

} // namespace mmx::deck_turret_parent
