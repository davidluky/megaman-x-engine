// autotest_script.cpp - drives deterministic frame inputs for autotest runs.
// Owns: profile-specific input timing and optional parity replay tapes.

#include "autotest_script.h"

#include "app/input.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace mmx::autotest_script {
namespace {

struct ParityReplayFrame {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool jump = false;
    bool dash = false;
    bool shoot = false;
};

void applySourceInputToken(ParityReplayFrame& frame, const std::string& token) {
    if (token == "left") frame.left = true;
    else if (token == "right") frame.right = true;
    else if (token == "up") frame.up = true;
    else if (token == "down") frame.down = true;
    else if (token == "b") frame.jump = true;
    else if (token == "a") frame.dash = true;
    else if (token == "y") frame.shoot = true;
    else if (token == "x") frame.shoot = true;
}

ParityReplayFrame parseSourceInputNames(std::string names) {
    ParityReplayFrame frame;
    if (names.empty()) return frame;
    for (char& ch : names) {
        if (ch == '+') ch = ' ';
    }
    std::istringstream stream(names);
    std::string token;
    while (stream >> token) {
        applySourceInputToken(frame, token);
    }
    return frame;
}

std::vector<ParityReplayFrame> loadParityReplayTape() {
    std::vector<ParityReplayFrame> frames;
    const char* path = std::getenv("MMX_PARITY_INPUT_REPLAY");
    if (!path || *path == '\0') return frames;
    std::ifstream in(path);
    if (!in.is_open()) return frames;

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first && line.rfind("frame,input_names", 0) == 0) {
            first = false;
            continue;
        }
        first = false;
        const auto comma = line.find(',');
        const std::string names = comma == std::string::npos ? line : line.substr(comma + 1);
        frames.push_back(parseSourceInputNames(names));
    }
    return frames;
}

void applyParityReplayFrame(const ParityReplayFrame& frame, bool resetEdges) {
    static bool prevJump = false;
    static bool prevDash = false;
    if (resetEdges) {
        prevJump = false;
        prevDash = false;
    }

    if (frame.left) Input::setScriptedLeftHeld(true);
    if (frame.right) Input::setScriptedRightHeld(true);
    if (frame.up) Input::setScriptedUpHeld(true);
    if (frame.down) Input::setScriptedDownHeld(true);
    if (frame.jump) Input::setScriptedJumpHeld(true);
    if (frame.dash) Input::setScriptedDashHeld(true);
    if (frame.shoot) Input::setScriptedShootHeld(true);

    if (frame.jump && !prevJump) Input::scriptedJumpPress();
    if (frame.dash && !prevDash) Input::scriptedDashPress();
    prevJump = frame.jump;
    prevDash = frame.dash;
}

} // namespace

void runFrame(std::string_view profile, int frame, const SnapFn& snap) {
    const int f = frame;

    Input::clearScripted();

    if (profile == "parity-probe") {
        // No scripted input. Used by U283 point probes that need the engine
        // state at a source-proven spawn/camera anchor.

    } else if (profile == "parity-replay" || profile == "buster-replay") {
        // File-driven source input replay. The tape is intentionally tiny and
        // test-only: CSV rows of `frame,input_names`, using the Mesen census
        // input_names column (`b`=jump, `a`=dash, `y`/`x`=shoot).
        static const std::vector<ParityReplayFrame> tape = loadParityReplayTape();
        const int index = f - 1;
        if (index >= 0 && index < static_cast<int>(tape.size())) {
            applyParityReplayFrame(tape[static_cast<size_t>(index)], f == 1);
        } else if (f == 1) {
            applyParityReplayFrame(ParityReplayFrame{}, true);
        }

    } else if (profile == "gates") {
        // T1.1 retained rightward replay, --spawn-at 7344,615. Snap both
        // sides of passage; source-synchronized parity lives in the CTest.
        // T1.1b extends it through the second door: from the retained
        // checkpoint (7520,615) the second record's open/walk/close needs
        // 420 + 84 more ticks than the first door's window.
        if (f >= 1 && f <= 560) Input::setScriptedRightHeld(true);
        snap(16);
        snap(68);
        snap(186);
        snap(270);
        snap(271);
        snap(420);
        snap(470);   // second record: the close is running
        snap(520);   // after the release
        snap(560);
    } else if (profile == "walk") {
        // Original timeline: walk right across two bursts, 5 snaps.
        bool right = (f >= 60  && f < 240) || (f >= 330 && f < 420);
        if (right) Input::setScriptedRightHeld(true);
        snap(30);
        snap(120);
        snap(180);
        snap(270);
        snap(360);

    } else if (profile == "warp") {
        // No input. With MEGAMAN_X_FORCE_WARP set, capture the source-clock
        // READY flashes, the one-frame blank handoff, warp, and settled X.
        snap(1);     // first READY frame (source 799)
        snap(14);    // last frame of the first visible run (source 812)
        snap(15);    // first hidden frame (source 813)
        snap(23);    // second visible run begins (source 821)
        snap(111);   // final READY frame (source 909)
        snap(112);   // blank handoff (source 910)
        snap(113);   // warp begins (source 911)
        snap(143);   // final warp / first responsive frame (source 941)
        snap(144);   // settled source pose (source 942)

    } else if (profile == "capsule") {
        // Candidate CP boots capsule smoke. The stage-file/spawn-at command
        // drops X on the U240 candidate anchor; no scripted input should be
        // required because the source-timed cutscene owns controls after
        // overlap. Snap source-proven dialog/grant/release moments.
        snap(30);     // overlap has entered the cutscene
        snap(660);    // U245 first mapped BG3 dialog plane
        snap(1320);   // U245 tail line wraps to the top BG3 page
        snap(2496);   // U244 boots grant event
        snap(2720);   // first rendered frame after control release

    } else if (profile == "helmet-capsule") {
        // FU7 Storm Eagle source-timed helmet capsule smoke. The command
        // starts X 21px left of the promoted pickup so hitboxes overlap.
        snap(1);      // source dialog begins
        snap(132);    // first typed dialog state
        snap(186);    // T13 matching dialog source screenshot (global 7350)
        snap(187);    // render of source-local 186 after the update edge
        snap(564);    // final dialog row
        snap(584);    // pedestal lock
        snap(585);    // render of source-local 584 after the update edge
        snap(842);    // T10 matching pedestal source screenshot (global 8006)
        snap(843);    // render of source-local 842 after the update edge
        snap(916);    // exact T09 helmet grant edge
        snap(917);    // render of source-local 916 after the grant update
        snap(1139);   // exact T09 control-return edge
        snap(1140);   // one-frame source-local 1139 release presentation

    } else if (profile == "body-capsule") {
        // FU7 Sting Chameleon source-playback body-capsule smoke. The command starts
        // X 21px left of the source interaction anchor. Right stays held to
        // prove ordinary input resumes after the measured release edge.
        Input::setScriptedRightHeld(true);
        snap(1);      // source-local 0 presentation after pickup start
        snap(139);    // rendered source-local 138 / source 408
        snap(140);    // rendered source-local 139 / first text blip
        snap(312);    // rendered source-local 311 / page-one complete
        snap(428);    // rendered source-local 427 / page-one scroll
        snap(460);    // rendered source-local 459 / page-two first blip
        snap(659);    // rendered source-local 658 / page-two complete
        snap(780);    // rendered source-local 779 / page-two scroll
        snap(872);    // rendered source-local 871 / dialog clear
        snap(934);    // semantic row before pedestal lock
        snap(935);    // rendered source-local 934 / pedestal lock
        snap(1189);   // semantic row before enhancement onset
        snap(1190);   // rendered source-local 1189 / enhancement onset
        snap(1253);   // exact body grant semantic edge
        snap(1254);   // rendered source-local 1253 / body grant
        snap(1459);   // exact control release/collection semantic edge
        snap(1460);   // rendered source-local 1459 / release presentation
        snap(1461);   // source 1730 / first converted-origin motion
        snap(1465);   // source 1734 / one-frame release-motion pause
        snap(1466);   // source 1735 / measured 376/256 run cadence
        snap(1474);   // source 1743 / clipped fractional wall contact
        snap(1531);   // source 1800 / stable held-Right wall tail

    } else if (profile == "shoot") {
        // Walk right the whole run. Fire a short buster shot every ~60
        // frames: hold shoot for 4 ticks (quick tap â€” fires a Normal shot),
        // then release. shootAnimTimer_ lasts 14 frames in Player, so a
        // snap 8 frames after release lands mid-shoot-pose reliably.
        if (f >= 60) Input::setScriptedRightHeld(true);

        // Fire bursts at 100, 200, 300, 400, 500.
        auto burstAt = [&](int startF) {
            if (f >= startF && f < startF + 4) Input::setScriptedShootHeld(true);
        };
        burstAt(100); burstAt(200); burstAt(300); burstAt(400); burstAt(500);

        // Capture ~8 frames after each release (release is at startF+4,
        // shoot overlay runs 14 frames from that point â€” snap at +10 keeps
        // the pose clearly visible).
        snap(30);   // resting spawn
        snap(114);  // mid walk_shoot burst 1
        snap(214);  // burst 2
        snap(314);  // burst 3
        snap(414);  // burst 4
        snap(514);  // burst 5
        snap(590);  // decay to plain walk

    } else if (profile == "combat") {
        // Walk right + dash + jump + shoot in varied combinations so every
        // _shoot overlay variant fires. Layout:
        //   30    spawn shot
        //   60    start walking right
        //   100   shoot burst (standing â€” actually mid-walk at this point)
        //   150   jump press; 154 shoot burst during rising jump -> jump_shoot
        //   220   dash press (ground dash starts)
        //   225   shoot burst during dash -> dash_shoot
        //   280   stop walking for a beat, shoot standing still -> shoot
        //   340   resume walking
        //   400   jump + hold shoot into fall -> jump_shoot held longer
        //   600   stop walking, settle
        //   shots at 30 / 120 / 160 (jump_shoot) / 230 (dash_shoot) / 290 (shoot)
        //         / 410 (jump_shoot w/ held fire) / 500 / 700 / 870
        if (f >= 60 && f < 280) Input::setScriptedRightHeld(true);
        if (f >= 340 && f < 600) Input::setScriptedRightHeld(true);

        if (f >= 100 && f < 104) Input::setScriptedShootHeld(true);
        if (f >= 150 && f < 151) Input::scriptedJumpPress();
        if (f >= 150 && f < 200) Input::setScriptedJumpHeld(true);  // held jump = higher apex
        if (f >= 154 && f < 158) Input::setScriptedShootHeld(true);
        if (f >= 220 && f < 221) Input::scriptedDashPress();
        if (f >= 220 && f < 238) Input::setScriptedDashHeld(true);
        if (f >= 225 && f < 229) Input::setScriptedShootHeld(true);
        if (f >= 290 && f < 294) Input::setScriptedShootHeld(true);
        if (f >= 400 && f < 401) Input::scriptedJumpPress();
        if (f >= 400 && f < 440) Input::setScriptedJumpHeld(true);
        if (f >= 404 && f < 430) Input::setScriptedShootHeld(true);  // held fire = charge

        // Jump apex timing: at jumpVelocity=-4.5, gravity=0.22 the apex sits
        // ~20 frames past the press. Press at 150 => apex at 171. Snap 170
        // lands at the peak where jump_shoot is most visibly airborne.
        snap(30);     // spawn
        snap(120);    // mid walk_shoot
        snap(170);    // rising jump + shoot -> jump_shoot at apex
        snap(232);    // dashing + shoot -> dash_shoot (dash is 18f long from 221)
        snap(295);    // standing shoot
        snap(420);    // jumping with charge held -- apex of 2nd jump
        snap(500);    // post-combo resume
        snap(700);    // late run
        snap(870);    // final rest

    } else if (profile == "weapons") {
        // Main.cpp/GameplayScene grant all 8 Maverick weapons for this
        // profile. Stay near spawn, cycle one slot at a time, fire each
        // weapon, and snap mid-shoot so X's active body tint and projectile
        // visual are both visible in the screenshot set.
        constexpr int kWeaponCount = 9; // buster + 8 Maverick weapons
        constexpr int kFirstBurst = 52;
        constexpr int kStride = 76;
        auto burstAt = [&](int startF) {
            if (f >= startF && f < startF + 4) Input::setScriptedShootHeld(true);
        };

        snap(30); // buster idle baseline
        for (int slot = 0; slot < kWeaponCount; ++slot) {
            const int burstFrame = kFirstBurst + slot * kStride;
            if (slot > 0 && f == burstFrame - 20) {
                Input::scriptedWeaponNextForce();
            }
            burstAt(burstFrame);
            snap(burstFrame + 14);
        }
        snap(730); // final slot settled after the last shot

    } else if (profile == "weapon-idle") {
        // Current weapon is selected by MMX_AUTOTEST_WEAPON_SLOT in
        // autotest_setup::grantAllWeapons. Keep input neutral so
        // HUD energy can settle for exact segment-gate captures.
        snap(30);
        snap(90);

    } else if (profile == "ice") {
        // Shotgun Ice fidelity drive (oracle campaign 2026-06-09): cycle to
        // Ice (last granted slot), tap once (press-fire pellet), charge past
        // chargeTimeSpecial (179 player frames; player ticks ~0.36x autotest
        // frames, so hold ~600), release -> sled forms, launches at +90,
        // accelerates to 4.0; X walks ahead and gets scooped; rides until the
        // CP slope/wall. Snaps document every phase.
        for (int i = 0; i < 8; ++i) {
            if (f == 30 + i * 4) Input::scriptedWeaponNextForce();
        }
        if (f >= 80 && f < 760) Input::setScriptedShootHeld(true);   // press + charge
        // Short walk: end ~35px ahead of the forming sled so it scoops X
        // shortly after its launch (launch â‰ˆ f1010 at the 0.36x tick ratio).
        if (f >= 770 && f < 830) Input::setScriptedRightHeld(true);

        snap(70);    // ice equipped, idle (gray/white tint)
        snap(90);    // press-fired pellet in flight (8 px/f)
        snap(400);   // mid charge aura
        snap(750);   // full charge held
        snap(790);   // released: sled forming, stationary
        snap(1000);  // formation/settle phase
        snap(1150);  // launched, accelerating
        snap(1300);  // scoop/ride window
        snap(1450);  // riding / approaching slope
        snap(1600);  // wall interaction outcome
        snap(1750);  // aftermath (sled gone or off-screen)

    } else if (profile == "ghost") {
        // Task-17 ghost proof on content/x1/stages/ghost-flat.json (floor at
        // y=192, wall face at x=896, X spawns at x=600). Drives every oracle
        // scenario in one run for the MMX_PROJTRACE frame trace:
        //   S1 right + left pellets in open air (exact 8.0 px/f, despawn),
        //   S2 pellet into the wall (5-fragment fan),
        //   S3 charge -> sled (90f formation, 0.0625 px/f^2 ramp, 4.0 cap),
        //   S4 X walks ahead and is scooped (exact dx coupling), sled breaks
        //   on the wall. ghost_diff.py asserts all of it from the trace.
        // Frame numbers are autotest frames (~0.36 physics ticks each).
        for (int i = 0; i < 8; ++i) {
            if (f == 30 + i * 4) Input::scriptedWeaponNextForce();   // equip ice
        }
        if (f >= 80 && f < 86) Input::setScriptedShootHeld(true);    // S1 right
        if (f >= 100 && f < 120) Input::setScriptedLeftHeld(true);   // turn left
        if (f >= 130 && f < 136) Input::setScriptedShootHeld(true);  // S1 left
        // Stop ~80px short of the wall face (x=896): a wall-PINNED X spawns
        // the pellet past the face (muzzle reaches ~+56px) and it never
        // impacts â€” first ghost-trace run caught exactly that.
        if (f >= 150 && f < 295) Input::setScriptedRightHeld(true);  // walk near wall
        if (f >= 630 && f < 636) Input::setScriptedShootHeld(true);  // S2 shatter
        if (f >= 660 && f < 900) Input::setScriptedLeftHeld(true);   // back off
        if (f >= 910 && f < 916) Input::setScriptedRightHeld(true);  // face right
        if (f >= 930 && f < 1570) Input::setScriptedShootHeld(true); // press + full charge
        if (f >= 1580 && f < 1750) Input::setScriptedRightHeld(true); // walk into scoop path
        snap(70);     // ice equipped
        snap(90);     // S1 right pellet
        snap(640);    // S2 impact / fragments
        snap(1560);   // full charge held
        snap(1600);   // sled forming
        // Sled moves ticks ~1660-1786 (launch -> wall break); the old
        // 1850/2000 snaps were AFTER the break and showed nothing.
        snap(1690);   // sled launched, snow spray active
        snap(1760);   // ride window, near the wall
        snap(1800);   // wall-break burst mid-flight (break ~1787)

    } else if (profile == "ghost-espark") {
        // Electric Spark ghost proof on content/x1/stages/ghost-flat.json
        // (floor y=192, wall face x=896, X spawns x=600). Scenarios:
        //   S1 left spark in open air (exact -3.0 px/f),
        //   S1 right spark into the wall -> terrain split: 2 children at
        //     (0,+-6.0) exactly, x constant, the down child PIERCES the floor,
        //   S3 charged twin fired LEFT (open both ways): forward giant
        //     stationary 12 ticks then -8.0; backward twin slot-claimed at
        //     +10, +8.0 toward the wall (wall contact not asserted â€” charged
        //     terrain interaction is oracle-unmeasured).
        // e-spark = 6 weapon-next presses (inventory order matches cursor c6).
        for (int i = 0; i < 6; ++i) {
            if (f == 30 + i * 4) Input::scriptedWeaponNextForce();
        }
        if (f >= 70 && f < 90) Input::setScriptedLeftHeld(true);     // face left
        if (f >= 100 && f < 106) Input::setScriptedShootHeld(true);  // S1 left
        if (f >= 180 && f < 200) Input::setScriptedRightHeld(true);  // face right
        if (f >= 210 && f < 216) Input::setScriptedShootHeld(true);  // S1 right (clean, dies at the margin)
        // The 3.0 px/f spark can't out-run the despawn margin from x=600 â€”
        // walk near the wall (x~800) so the split shot actually reaches it.
        if (f >= 230 && f < 370) Input::setScriptedRightHeld(true);
        if (f >= 400 && f < 406) Input::setScriptedShootHeld(true);  // split shot -> wall ~f418
        if (f >= 470 && f < 560) Input::setScriptedLeftHeld(true);   // back off + face left
        if (f >= 590 && f < 790) Input::setScriptedShootHeld(true);  // press + full charge (200 > 179)
        snap(60);     // e-spark equipped
        snap(215);    // S1 right spark in flight
        snap(420);    // wall split children on the face
        snap(810);    // charged twins launched

    } else if (profile == "ghost-cutter") {
        // Boomerang Cutter ghost proof on ghost-flat.json (open floor, X
        // spawns x=600). Scenarios: S1 right throw + stationary catch, S1
        // left throw + catch, S3 charged release (4 giants; the down one
        // dies on the floor â€” terrain death, vector-only assert).
        // cutter = 7 weapon-next presses (inventory order matches cursor c7).
        for (int i = 0; i < 7; ++i) {
            if (f == 30 + i * 4) Input::scriptedWeaponNextForce();
        }
        if (f >= 80 && f < 100) Input::setScriptedRightHeld(true);   // face right
        if (f >= 110 && f < 116) Input::setScriptedShootHeld(true);  // S1 right (catch ~f185)
        if (f >= 240 && f < 260) Input::setScriptedLeftHeld(true);   // face left
        if (f >= 270 && f < 276) Input::setScriptedShootHeld(true);  // S1 left (catch ~f345)
        if (f >= 400 && f < 600) Input::setScriptedShootHeld(true);  // press + full charge (200 > 179)
        snap(60);     // cutter equipped
        snap(150);    // S1 right mid-arc
        snap(310);    // S1 left mid-arc
        snap(620);    // charged giants launched

    } else if (profile == "ghost-torpedo") {
        // Homing Torpedo ghost proof on ghost-torpedo.json (ghost-flat
        // geometry + ONE stationary ghost_target dummy at x=820; X spawns x=600).
        // Scenarios:
        //   S1 right: torpedo launches 7f straight then homes onto the
        //     cannon (forward pursuit, shallow bearing),
        //   S1 left: target is BEHIND -> folded steering, per-axis brake,
        //     turnaround, pursuit back to the cannon,
        //   S3 charged release: 5-way fan on the NO-TARGET branch of the
        //     homing gate (w70iso_REPORT: members steer iff their acquired
        //     target is alive at countdown expiry) — the dummy must be DEAD
        //     by the release. Under the MEASURED rigid camera (anchor-128,
        //     no lead — U10 camera law) the dummy at x=820 sits past the
        //     view's right edge while X stands at spawn, so eligibility
        //     drops mid-flight and distant shots MISS (the old eased+lead
        //     camera kept it eligible — scenario, not engine, relied on
        //     that). X now walks close first, then three kill shots land.
        // torpedo = 1 weapon-next press (inventory order matches cursor c1).
        if (f == 30) Input::scriptedWeaponNextForce();
        if (f >= 80 && f < 100) Input::setScriptedRightHeld(true);   // face right
        if (f >= 110 && f < 116) Input::setScriptedShootHeld(true);  // S1 right
        if (f >= 260 && f < 280) Input::setScriptedLeftHeld(true);   // face left
        if (f >= 290 && f < 296) Input::setScriptedShootHeld(true);  // S1 left (turnaround)
        if (f >= 410 && f < 490) Input::setScriptedRightHeld(true);  // walk close (dummy on-screen)
        if (f >= 495 && f < 501) Input::setScriptedShootHeld(true);  // kill shot (8->5)
        if (f >= 525 && f < 531) Input::setScriptedShootHeld(true);  // kill shot (5->2)
        if (f >= 555 && f < 561) Input::setScriptedShootHeld(true);  // kill shot (2->dead)
        if (f >= 600 && f < 800) Input::setScriptedShootHeld(true);  // press + full charge (200 > 179)
        snap(60);     // torpedo equipped
        snap(160);    // S1 right homing toward the cannon
        snap(360);    // S1 left turned around
        snap(820);    // charged fan launched

    } else if (profile == "ghost-sting") {
        // Chameleon Sting ghost proof on ghost-torpedo.json (the dummy at
        // x=820 serves the charged-immunity walk). Scenarios:
        //   S1 right: muzzle bolt 23f at anchor+(16,-3), 3-dart fan at
        //     tick 20 (exact claim offsets/vectors; straight dart hits the
        //     dummy, -2),
        //   S1 left: mirrored volley, margin-dies left,
        //   S3 charged release: NO projectile — the invincibility state;
        //     X then walks right THROUGH the live dummy with no knockback
        //     (player-trace monotonic x = the end-to-end immunity check).
        // sting = 2 weapon-next presses (inventory order matches cursor c2).
        if (f == 30) Input::scriptedWeaponNextForce();
        if (f == 34) Input::scriptedWeaponNextForce();
        if (f >= 80 && f < 100) Input::setScriptedRightHeld(true);   // face right
        if (f >= 110 && f < 116) Input::setScriptedShootHeld(true);  // S1 right
        if (f >= 260 && f < 280) Input::setScriptedLeftHeld(true);   // face left
        if (f >= 290 && f < 296) Input::setScriptedShootHeld(true);  // S1 left
        if (f >= 400 && f < 600) Input::setScriptedShootHeld(true);  // press + full charge (200 > 179)
        if (f >= 620 && f < 900) Input::setScriptedRightHeld(true);  // walk through the dummy, immune
        snap(60);     // sting equipped
        snap(140);    // S1 right fan in flight
        snap(320);    // S1 left fan in flight
        // U03b visual-cycle verify. Release = f600; snap(N) captures
        // the framebuffer BEFORE frame N renders, i.e. render N-1 → snap at
        // 600+d+1 shows visual frame d (verified empirically: snap 610
        // showed the d9 gold phase).
        snap(603);    // d2: normal palette window (visible)
        snap(610);    // d9: phase b GOLD (visible)
        snap(611);    // d10: blink — X fully hidden
        snap(634);    // d33: phase f BLUE (visible)
        snap(641);    // d40: hidden frame while standing (regime 1)
        snap(790);    // inside the dummy contact window (d~189)

    } else if (profile == "ghost-firewave") {
        // Fire Wave ghost proof on ghost-torpedo.json (the dummy at x=820
        // stays beyond the 51px stream reach from spawn — no interaction).
        // Scenarios (oracle _fire_wave_runs 2026-06-11):
        //   S1 stream: 30f hold -> a segment every 2f (first interval 3f),
        //     each +-7.0 px/f for exactly 10 ticks,
        //   S3 charged: 240f hold (>179) -> release plants the stationary
        //     head + the ground wave chain (+12 then +15px every 10f, 40f
        //     segment lives, 25 segments on the flat floor).
        // fire-wave = 4 weapon-next presses (inventory order = cursor c4).
        if (f == 30) Input::scriptedWeaponNextForce();
        if (f == 34) Input::scriptedWeaponNextForce();
        if (f == 38) Input::scriptedWeaponNextForce();
        if (f == 42) Input::scriptedWeaponNextForce();
        if (f >= 80 && f < 100) Input::setScriptedRightHeld(true);   // face right
        if (f >= 110 && f < 140) Input::setScriptedShootHeld(true);  // stream burst
        if (f >= 200 && f < 440) Input::setScriptedShootHeld(true);  // press + full charge (240 > 179)
        snap(60);     // fire-wave equipped
        snap(130);    // stream in flight
        snap(520);    // wave chain mid-crawl
        snap(700);    // chain near the cap

    } else if (profile == "ghost-stormtornado") {
        // Storm Tornado ghost proof on ghost-torpedo.json. Scenarios
        // (oracle _storm_tornado_runs 2026-06-11):
        //   S1 tap: the column grows WORLD-FIXED 65f then rushes at 8px/f
        //     until 32px past the screen edge,
        //   S3 charged: 240f hold (>179) -> release spawns the TWO
        //     stationary halves at X +-(0,96), both 55f.
        // storm-tornado = 5 weapon-next presses (inventory order = c5).
        if (f == 30) Input::scriptedWeaponNextForce();
        if (f == 34) Input::scriptedWeaponNextForce();
        if (f == 38) Input::scriptedWeaponNextForce();
        if (f == 42) Input::scriptedWeaponNextForce();
        if (f == 46) Input::scriptedWeaponNextForce();
        if (f >= 80 && f < 100) Input::setScriptedRightHeld(true);   // face right
        if (f >= 110 && f < 116) Input::setScriptedShootHeld(true);  // S1 column
        if (f >= 260 && f < 500) Input::setScriptedShootHeld(true);  // press + full charge (240 > 179)
        // U30: the tornado draws on EVEN ages only (flicker) — snap(N)
        // shows render N-1, fire f110/release f500 -> N odd lands even ages.
        snap(60);     // storm-tornado equipped
        snap(161);    // column growing in place (age 50)
        snap(187);    // column rushing (age 76)
        snap(509);    // halves growing (age 8)
        snap(531);    // twin halves swirl loop (age 30)

    } else if (profile == "ghost-rollingshield") {
        // Rolling Shield ghost proof on ghost-torpedo.json. Scenarios
        // (oracle _rolling_shield_runs 2026-06-11):
        //   S1 tap NEAR THE WALL (stage wall at col 56 = x896): X walks
        //     right first so the ball reaches the face inside its 32px
        //     margin life — 7f spawn pause, exact 4.0 px/f roll, then the
        //     U42 WALL BOUNCE (exact vx flip at the face, keeps rolling),
        //   S3 charged: 240f hold (>179) -> release spawns the X-GLUED
        //     shield; X then walks so the glue check exercises motion.
        // rolling-shield = 3 weapon-next presses (inventory order = c3).
        if (f == 30) Input::scriptedWeaponNextForce();
        if (f == 34) Input::scriptedWeaponNextForce();
        if (f == 38) Input::scriptedWeaponNextForce();
        if (f >= 60 && f < 190) Input::setScriptedRightHeld(true);   // approach the wall
                                                                     // (stop ~x750: the ball
                                                                     // lands BEFORE the face
                                                                     // = grounded bounce)
        if (f >= 220 && f < 226) Input::setScriptedShootHeld(true);  // S1 ball -> bounce
        if (f >= 330 && f < 570) Input::setScriptedShootHeld(true);  // press + full charge (240 > 179)
        if (f >= 600 && f < 700) Input::setScriptedRightHeld(true);  // walk with the shield glued
        snap(60);     // rolling-shield equipped
        snap(250);    // ball rolling at the wall
        snap(640);    // shield glued, X walking

    } else if (profile == "die") {
        // No scripted input. Main.cpp places X at y=50000 so the first
        // handlePlayerDeath tick kills them via the pit check; we just
        // snap at each ring spawn + the respawn moment so the whole
        // DD orb burst schedule ends up in screenshots.
        //
        // deathTimer -> autotestFrame mapping: death enters on frame 1,
        // deathTimer increments in updateDie on frames 2+. So snap at
        // autotestFrame N captures deathTimer = N-1 (roughly).
        //
        //   frame 1   â€” spawn (falling, not yet dead this physics)
        //   frame 32  â€” ring 1 just spawned (deathTimer 31)
        //   frame 33  â€” ring 2 spawned (dense 16-orb burst visible)
        //   frame 65  â€” ring 3 (deathTimer 64)
        //   frame 99  â€” ring 4 (deathTimer 98)
        //   frame 142 â€” ring 5 (deathTimer 141)
        //   frame 185 â€” ring 6 (deathTimer 184)
        //   frame 221 â€” respawn just fired (deathTimer 220 -> checkpoint)
        //   frame 240 â€” X back in play at checkpoint
        snap(1);
        snap(32);
        snap(33);
        snap(65);
        snap(99);
        snap(142);
        snap(185);
        snap(221);
        snap(240);
        snap(282); // final-stock run: first password-grid frame
        snap(320); // final-stock run: settled password-grid presentation
        if (f == 325) Input::scriptedJumpPress();
        snap(360); // final-stock run: destination transition
        snap(390); // final-stock run: destination fade
        snap(450); // final-stock run: destination reveal
        snap(500); // final-stock run: settled destination

    } else if (profile == "boss-entry") {
        // CP-B1A review frames. Boss-room spawn activates on autotest f1,
        // which maps to source t84; each later rendered frame advances the
        // source clock once. No combat input is injected during the intro.
        snap(1);    // source t84: object live, body hidden; entry pan/walk start
        // CP-B1C-T1-M2 entry-motion review ladder: camera pans +2 px/tick
        // into the lock (t116 = f33) while X walks +116/256 px/tick into
        // the settle (t117 = f34).
        snap(9);    // source t92: pan/walk first quarter
        snap(17);   // source t100: pan/walk midpoint
        snap(25);   // source t108: pan/walk third quarter
        snap(33);   // source t116: camera lock tick
        snap(34);   // source t117: walk settle tick
        snap(181);  // source t264: fall starts
        snap(217);  // source t300: landed
        snap(257);  // source t340: displayed HP 1 + first $0C
        snap(319);  // source t402: displayed HP 32
        snap(326);  // source t409: fight-on / IdleGate
        snap(335);  // stable early-fight hold

    } else if (profile == "boss") {
        // Walk right to enter the boss room (auto-spawned 128px left of the
        // boss by main.cpp), then stand and shoot to exercise the fight.
        // Timeline: 0-60 idle, 60-180 walk right (trigger room lock),
        // 180+ stand and shoot in bursts.
        if (f >= 60 && f < 180)  Input::setScriptedRightHeld(true);
        if (f >= 240 && f < 244) Input::setScriptedShootHeld(true);
        if (f >= 340 && f < 344) Input::setScriptedShootHeld(true);
        if (f >= 440 && f < 444) Input::setScriptedShootHeld(true);
        if (f >= 540 && f < 544) Input::setScriptedShootHeld(true);
        if (f >= 640 && f < 644) Input::setScriptedShootHeld(true);
        if (f >= 740 && f < 744) Input::setScriptedShootHeld(true);
        if (f >= 840 && f < 844) Input::setScriptedShootHeld(true);
        if (f >= 940 && f < 944) Input::setScriptedShootHeld(true);

        snap(30);    // pre-fight (spawn)
        snap(120);   // entering boss room
        snap(200);   // intro / HP bar visible
        snap(350);   // early fight
        snap(550);   // mid fight
        snap(750);   // mid-late fight
        snap(950);   // late fight (phase 2 if boss has one)
        snap(1150);  // near end

    } else if (profile == "boss-l3") {
        // Boss L3 smoke: charge through the real player path, jump to clear
        // Chill Penguin's arena collision band, then release into the active
        // boss hitbox so MMX_PROJTRACE can prove HP 32 -> 29.
        if (f >= 10 && f < 435) Input::setScriptedShootHeld(true);
        if (f == 420) Input::scriptedJumpPress();
        if (f >= 420 && f < 470) Input::setScriptedJumpHeld(true);
        snap(300);   // full charge held
        snap(430);   // jump alignment
        snap(437);   // release flash
        snap(445);   // first moving L3 object
        snap(465);   // expected boss overlap window
        snap(520);   // blink-gated aftermath

    } else if (profile == "slopes") {
        // Just walk right for a very long time, enough to cross Chill
        // Penguin's slope section (col 494 = x 7904; at 1.5px/frame that's
        // ~5270 frames from spawn). Take many shots along the way.
        if (f >= 60) Input::setScriptedRightHeld(true);
        snap(30);
        snap(500);
        snap(1500);
        snap(3000);
        snap(5000);
        snap(5500);
        snap(6000);
        snap(6500);
        snap(7000);
        snap(7500);

    } else if (profile == "charge2") {
        // Hold shoot long enough to reach level 2 (chargeTime2=100), then release
        // and snap the fired shot in flight to inspect its sprite/color. NOTE: the
        // player updates at ~0.36x the autotest frame rate (stage intro + tick
        // model), so chargeTimer lags the autotest frame; hold to ~f330 so the
        // chargeTimer clears 100 (was f130 -> only reached L1, fired green).
        if (f >= 10 && f < 330) Input::setScriptedShootHeld(true);
        snap(180);   // mid charge (L1 aura)
        snap(300);   // full charge held (L2 aura)
        snap(336);   // just fired (L2 shot)
        snap(340);
        snap(345);
        snap(350);

    } else if (profile == "wallslide") {
        // Spawn X in the air next to a right-side wall (use --spawn-at), hold
        // RIGHT so he drifts into the wall and wall-slides. Snap across the
        // descent so we can read facing direction + descent speed (flicker shows
        // as uneven Y spacing / facing flips).
        Input::setScriptedRightHeld(true);
        // After X is pinned and sliding, attempt wall jumps (press jump). In MMX
        // you press jump while holding toward the wall to kick off.
        if (f == 50 || f == 80 || f == 110) Input::scriptedJumpPress();
        for (int s = 10; s <= 150; s += 8) snap(s);

    } else if (profile == "ladder") {
        // Spawn X with his hitbox center on a ladder tile (use --spawn-at),
        // then hold UP so runtime traces and screenshots cover the climb.
        Input::setScriptedUpHeld(true);
        snap(10);
        snap(20);
        snap(40);
        snap(60);
        snap(80);
        snap(90);
        snap(120);
        snap(150);
        snap(180);

    } else if (profile == "weapon-get") {
        // GC2.1b: the post-boss weapon-get sequence. No input at all - it is a
        // cutscene, and the source has no prompt to answer. The scene enters at
        // stage-clear tick 0, and the stage-clear branch advances one tick per
        // frame, so script frame f lands on sequence tick f - 1. Every snap
        // below is a measured beat from
        // knowledge_base/mmx1/story/weapon_get_sequence.json.
        snap(30);    // victory pose, banner fading in
        snap(58);    // f17876: warp-out starts
        snap(70);    // mid warp-out, on the {10, 11} px ladder
        snap(81);    // f17899: warp-out ends
        snap(173);   // f17991: the spec screen takes the frame
        snap(356);   // f18174: the first typed character + its $0B blip
        snap(400);   // typing, palette pair A alternating
        snap(464);   // f18282: palette pair B takes over
        snap(556);   // f18374: warp-in starts, 8 px/tick
        snap(572);   // f18390: warp-in ends, the demo pose
        snap(593);   // f18411: first demo shot
        snap(605);   // the same shot in flight, so travel is visible
        snap(633);   // f18451: second demo shot
        snap(673);   // f18491: third demo shot
        snap(899);   // f18717: the return handoff begins
        snap(931);   // f18750: source password grid takes the frame
        snap(1131);  // f18950: last captured password-grid frame
        snap(1146);  // Storm f21054: last captured password-grid frame
        snap(1147);  // source tails end; stage select owns the next frame

    } else if (profile == "intro") {
        // Boss-intro cutscene: no input. Deterministic 1 tick/frame, so
        // the script frame matches the scene timer. Snaps trace the entrance beats.
        snap(20);    // animated spark
        snap(66);    // white flash
        snap(90);    // boss materializing
        snap(120);   // star assembling (triangle dropping in)
        snap(150);   // assembled, name appearing
        snap(165);   // name in
        snap(210);   // HOLD (exact reveal)
        snap(300);   // HOLD

    } else {
        // Unknown profile â€” behave like walk but still exit cleanly.
        if (f >= 60 && f < 420) Input::setScriptedRightHeld(true);
        snap(30);
        snap(180);
        snap(360);
    }
}

} // namespace mmx::autotest_script
