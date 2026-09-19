#pragma once

// OID53 child OID23 pool controller; attack_child_law.json under oid_0x53_mad_pecker. Source: 80:D48D/D4D3,
// 87:DFCD..E036 allocation, 87:8968/8972/89A2, 82:820A/80B4/83A3,
// 84:8EEA/8F07, and AF:DAE8/DB16. The record is literal 64-byte WRAM.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mmx::mad_pecker_shot {

using Record = std::array<std::uint8_t, 0x40>;
using Pool = std::array<Record, 8>;

inline std::uint16_t word(const Record& r, std::size_t at) {
  return static_cast<std::uint16_t>(r[at] | (static_cast<std::uint16_t>(r[at + 1]) << 8));
}
inline void set_word(Record& r, std::size_t at, std::uint16_t value) {
  r[at] = static_cast<std::uint8_t>(value);
  r[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

struct ParentAllocation {
  std::uint16_t x = 0;       // parent +05/+06 at 87:DFCD
  std::uint16_t y = 0;       // parent +08/+09
  std::uint16_t x_speed = 0; // parent +1A/+1B
  std::uint16_t y_speed = 0; // parent +1C/+1D
  std::uint8_t gfx_slot18 = 0;
  std::uint8_t attr11 = 0;
  std::uint8_t attr34 = 0;
  std::uint8_t selector16 = 0; // OID53 source parent supplies $88
};

struct FrameEnvironment {
  std::uint16_t camera_x = 0;
  std::uint16_t camera_y = 0;
};

// 84:9B03 is a read-only operation on this OID23 record: it reads +27, +0E,
// +20 and, on a hit, +26.  84:9C0E writes only global scratch and 84:9D07
// writes Player/global damage state.  Thus the raw controller has no contact
// callback and cannot be reseeded by a post-contact expected record.

struct StepResult {
  bool initialized = false;
  bool contact_called = false;
  bool cleared = false;
  bool culled = false;
  bool unsupported_animation = false;
};

class PoolController {
 public:
  explicit PoolController(Pool initial = {}) : pool_(initial) {}
  const Pool& pool() const { return pool_; }
  Pool& pool() { return pool_; }

  // 82:8358 eligibility. Allocation chooses the first active word +00/+01==0.
  std::optional<std::size_t> first_free_slot() const {
    for (std::size_t i=0;i<pool_.size();++i) if (word(pool_[i],0x00)==0) return i;
    return std::nullopt;
  }

  // 87:DFCD..E036. Fractions +04 and +07, +28, and every unnamed field retain
  // their selected-slot residue. Parent's state-04 edge is external to pool law.
  std::optional<std::size_t> allocate_from_parent(const ParentAllocation& parent) {
    const auto slot=first_free_slot(); if(!slot) return std::nullopt;
    Record& r=pool_[*slot];
    set_word(r,0x00,static_cast<std::uint16_t>(word(r,0x00)+1)); // INC.W $0000,X
    r[0x0A]=0x23; r[0x18]=parent.gfx_slot18;
    r[0x11]=static_cast<std::uint8_t>(parent.attr34 | parent.attr11);
    r[0x16]=parent.selector16; r[0x0B]=0x02;
    set_word(r,0x05,parent.x);                                 // +0002 is zero
    set_word(r,0x08,static_cast<std::uint16_t>(parent.y + 16)); // Y - FFF0
    set_word(r,0x1A,parent.x_speed); set_word(r,0x1C,parent.y_speed);
    return slot;
  }

  // One 80:D48D child dispatch. The caller invokes it only for records present
  // before later parent allocation, matching source pool traversal order.
  StepResult dispatch(std::size_t slot, const FrameEnvironment& env) {
    Record& r=pool_.at(slot);
    // 80:D495 tests only the low active byte here.  Allocation uses the full
    // word at 82:8358/87:DFD5, so retain that deliberate source distinction.
    if(r[0x00]==0 || r[0x0A]!=0x23) return {};
    prepare_80d4d3(r);
    if(r[0x01]==0) return init_878972(r,env);
    if(r[0x01]!=2) return {};
    return live_8789a2(r,env);
  }

 private:
  Pool pool_;

  static void prepare_80d4d3(Record& r) {
    // Copies position for generic paths and `TRB #$80,$0E` before wrapper F88D.
    set_word(r,0x22,word(r,0x05)); set_word(r,0x24,word(r,0x08));
    r[0x0E]=static_cast<std::uint8_t>(r[0x0E] & 0x7Fu);
  }
  static void camera_8280b4(Record& r,const FrameEnvironment& env) {
    const auto x=static_cast<std::uint16_t>(word(r,0x05)-env.camera_x+0x20);
    const auto y=static_cast<std::uint16_t>(word(r,0x08)-env.camera_y+0x10);
    r[0x0E]=(x<0x0140 && y<0x0100)?0x81:0x00;
  }
  static void clear_8283a3(Record& r) {
    // REP #$20; STZ $00,$02,$0E,$2C; SEP #$20.
    set_word(r,0x00,0); set_word(r,0x02,0); set_word(r,0x0E,0); set_word(r,0x2C,0);
  }
  static bool load_selector88_subtype2(Record& r) {
    // `LDA $0B; JSL 84:8F07` in 87:8991. For OID53 allocation it selects
    // selector +16=$88, subtype +0B=2: AF:DB16 = 08 80 0A; FD FF loop.
    if(r[0x16]!=0x88 || r[0x0B]!=0x02) return false;
    r[0x13]=0x08; set_word(r,0x14,0xDB16); r[0x0F]=0x80; r[0x17]=0x8A;
    return true;
  }
  static void tick_selector88_subtype2(Record& r) {
    // 84:8EEA consumes the DB16 token, then AF:DB19 `FD FF` returns to DB16.
    r[0x13]=static_cast<std::uint8_t>(r[0x13]-1); // source DEC wraps at zero
    if(r[0x13]!=0) return;
    r[0x13]=0x08; set_word(r,0x14,0xDB16); r[0x0F]=0x80; r[0x17]=0x8A;
  }
  static void motion_82820a(Record& r) {
    // 82:820A preserves flags, adds X 16-bit fraction/speed then corrects +06;
    // it subtracts Y speed from +07/+08 and corrects +09 for signed borrow.
    const std::uint32_t xsum=static_cast<std::uint32_t>(word(r,0x04))+word(r,0x1A);
    set_word(r,0x04,static_cast<std::uint16_t>(xsum));
    const std::uint8_t xcarry=xsum>0xFFFFu?1u:0u;
    const std::uint8_t xadjust=(r[0x1B]&0x80u)?0xFFu:0u;
    r[0x06]=static_cast<std::uint8_t>(r[0x06]+xadjust+xcarry);

    const std::uint16_t before=word(r,0x07);
    const std::uint16_t speed=word(r,0x1C);
    const std::uint16_t ysum=static_cast<std::uint16_t>(before-speed);
    set_word(r,0x07,ysum);
    const std::uint8_t yborrow=before<speed?1u:0u;
    const std::uint8_t yadjust=(r[0x1D]&0x80u)?0xFFu:0u;
    // Source stores -(signed high) minus the SEC/SBC borrow into the high byte.
    r[0x09]=static_cast<std::uint8_t>(r[0x09]-yadjust-yborrow);
  }
  static StepResult init_878972(Record& r,const FrameEnvironment& env) {
    StepResult out; out.initialized=true;
    r[0x01]=2; r[0x27]=2; r[0x26]=2; r[0x12]=6; r[0x28]=1;
    set_word(r,0x20,0xC3A2); camera_8280b4(r,env);
    if(!load_selector88_subtype2(r)) { out.unsupported_animation=true; return out; }
    r[0x38]=0x40; // SFX $34 omitted: no audio side effect in this pure law.
    return out;
  }
  static StepResult live_8789a2(Record& r,const FrameEnvironment& env) {
    StepResult out;
    r[0x38]=static_cast<std::uint8_t>(r[0x38]-1);
    if(r[0x38]==0) { clear_8283a3(r); out.cleared=true; return out; }
    out.contact_called=true; // 87:89A6 -> 84:9B03; it cannot alter this record.
    motion_82820a(r);
    if(r[0x16]==0x88 && r[0x0B]==0x02) tick_selector88_subtype2(r);
    else out.unsupported_animation=true; // no guessed alternate stream
    camera_8280b4(r,env);
    if(r[0x0E]==0) { clear_8283a3(r); out.cleared=true; out.culled=true; }
    return out;
  }
};

} // namespace mmx::mad_pecker_shot
