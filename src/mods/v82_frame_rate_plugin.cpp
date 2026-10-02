/* High frame rate for Vigilante 8: 2nd Offense: true in-between frames drawn
 * by the game's own code (psxrecomp docs/RENDER_PASSES.md), not a crossfade.
 *
 * Frame loop (frame function 0x800132CC, 1P): logic ticks, then the 1P draw
 * section from 0x800143FC: camera (0x8002D278 -> 0x8002E0E8), packet arena
 * 0x80014B3C(gp+0x24), world OT 0x80031678, HUD into the one-entry OT at
 * sp+0x68, 0x80014D94 (wait for the previous flip, gp+0x614), DrawSync and
 * submit 0x80014D00. Submit draws the HUD list at once, onto the buffer the
 * previous frame's world was drawn into, and arms a VBlank callback
 * (0x80014BE4) that flips to that buffer (DISPENV[i] shows the buffer
 * DRAWENV[1-i] draws) and only then draws this frame's world OT into the other
 * buffer. So at the entry of 0x80014D94 of frame k+1 -- the render-pass point
 * -- world(k) is complete in buffer 1-gp+0x24 and waits for the flip that
 * follows k+1's submit; the flip shows world(k) + HUD(k+1).
 *
 * Per frame the plugin records the registers at 0x800143FC (instruction
 * hook), the camera the section hands 0x8002E0E8, and at 0x80014D94 the local
 * matrix of every object reachable from the draw lists and every particle
 * position: the state world(k+1) was built from. Each pass writes the k -> k+1
 * blend of those, replays the section up to the submit with
 * psx_mod_run_guest_span from the recorded registers (the camera hook swaps in
 * the blended camera; the game's own branches -- HUD, messages, pause menu,
 * replays -- run as they did), then draws the world OT and the HUD list into
 * world(k)'s buffer, in the order the screen gets them. The pass sandbox
 * restores the machine afterwards.
 */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_cycle_freeze.h"
#include "psx_netplay.h"
#include "gpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>


namespace {
constexpr const char* Package = "v82.enhancement.frame-rate";
constexpr const char* Feature = "frame-rate";
constexpr const char* Plugin = "v82.frame-rate";

constexpr uint32_t GP = 0x8006A7F0;
constexpr uint32_t WaitFlip = 0x80014D94;        // hook: entry
constexpr uint32_t WaitFlipReturn1P = 0x800146B4; // jal 0x80014D94 at 0x800146AC
constexpr uint32_t SectionStart = 0x800143FC, SectionStartWord = 0x83820031; // lb v0,0x31(gp)
constexpr uint32_t SectionEnd = 0x80014734;       // jal 0x80014D00 (submit)
constexpr uint32_t SetCamera = 0x8002E0E8;        // hook: entry (a0 world camera, a1 focal)
constexpr uint32_t DispEnvs = 0x8006C240, DrawEnvs = 0x8006BEA8;
constexpr uint32_t SetDrawEnv = 0x8005CDD4, DrawOTag = 0x8005C57C, DrawSync = 0x8005BF88;
constexpr uint32_t ActorTableOffset = 0x4818;     // -0x4818($fp)
constexpr uint32_t SplitStart[2] = {0x80013E90, 0x800141FC};
constexpr uint32_t SplitEnd[2] = {0x80014154, 0x80014358};
constexpr uint32_t SplitDone = 0x800143F4;
constexpr uint32_t SplitSynced = 0x8001415C;

constexpr unsigned MaxObjects = 4096, MaxParticles = 4096, MaxDepth = 16;
constexpr unsigned MaxPhases = 16;
// World coordinates span about 2^27 units per arena axis. Measured per frame
// in battle (log2 histograms, v82.fr.*_move_log2_*): vehicles and the camera
// move up to 2^15, projectiles up to 2^16, over 1-3 ticks. Beyond these per
// tick the object or camera was placed (respawn, cut), not moved, and is drawn
// where it is now.
constexpr double TeleportPerTick = 65536.0;
constexpr double CameraCutPerTick = 131072.0;
constexpr double MaxRotationRadians = 2.1;   // ~120 degrees between frames

uint32_t rd(uint32_t p) { return psx_mod_read_word(p); }
void wr(uint32_t p, uint32_t v) { psx_mod_write_word(p, v); }
uint16_t rh(uint32_t p) { return psx_mod_read_half(p); }
void wh(uint32_t p, uint16_t v) { psx_mod_write_half(p, v); }
uint8_t rb(uint32_t p) { return psx_mod_read_byte(p); }
bool ram(uint32_t p) { return (p & 3) == 0 && (p & 0xE0000000u) == 0x80000000u && (p & 0x1FFFFFFF) < 0x800000; }

struct Matrix { int16_t r[9]; int32_t t[3]; };

Matrix read_matrix(uint32_t p) {
    Matrix m;
    for (int i = 0; i < 9; ++i) m.r[i] = int16_t(rh(p + 2 * i));
    for (int i = 0; i < 3; ++i) m.t[i] = int32_t(rd(p + 0x14 + 4 * i));
    return m;
}
void write_matrix(uint32_t p, const Matrix& m, bool rotation) {
    if (rotation) for (int i = 0; i < 9; ++i) wh(p + 2 * i, uint16_t(m.r[i]));
    for (int i = 0; i < 3; ++i) wr(p + 0x14 + 4 * i, uint32_t(m.t[i]));
}

struct Quat { double w, x, y, z; };

// Rows of a 4.12 rotation must be unit and orthogonal for slerp to be exact;
// scaled or sheared matrices blend elementwise instead.
bool orthonormal(const Matrix& m) {
    for (int i = 0; i < 3; ++i) {
        double len = 0;
        for (int j = 0; j < 3; ++j) len += double(m.r[3 * i + j]) * m.r[3 * i + j];
        if (std::fabs(std::sqrt(len) - 4096.0) > 4096.0 * 0.04) return false;
        for (int k = i + 1; k < 3; ++k) {
            double dot = 0;
            for (int j = 0; j < 3; ++j) dot += double(m.r[3 * i + j]) * m.r[3 * k + j];
            if (std::fabs(dot) > 4096.0 * 4096.0 * 0.04) return false;
        }
    }
    return true;
}
Quat to_quat(const Matrix& m) {
    double a[9];
    for (int i = 0; i < 9; ++i) a[i] = m.r[i] / 4096.0;
    Quat q;
    const double tr = a[0] + a[4] + a[8];
    if (tr > 0) {
        double s = std::sqrt(tr + 1.0) * 2;
        q = {0.25 * s, (a[7] - a[5]) / s, (a[2] - a[6]) / s, (a[3] - a[1]) / s};
    } else if (a[0] > a[4] && a[0] > a[8]) {
        double s = std::sqrt(1.0 + a[0] - a[4] - a[8]) * 2;
        q = {(a[7] - a[5]) / s, 0.25 * s, (a[1] + a[3]) / s, (a[2] + a[6]) / s};
    } else if (a[4] > a[8]) {
        double s = std::sqrt(1.0 + a[4] - a[0] - a[8]) * 2;
        q = {(a[2] - a[6]) / s, (a[1] + a[3]) / s, 0.25 * s, (a[5] + a[7]) / s};
    } else {
        double s = std::sqrt(1.0 + a[8] - a[0] - a[4]) * 2;
        q = {(a[3] - a[1]) / s, (a[2] + a[6]) / s, (a[5] + a[7]) / s, 0.25 * s};
    }
    double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}
int16_t fixed(double v) { return int16_t(std::lround(std::clamp(v * 4096.0, -32768.0, 32767.0))); }

// False when the two orientations are too far apart to be one motion.
bool blend_rotation(const Matrix& a, const Matrix& b, double t, Matrix& out) {
    if (orthonormal(a) && orthonormal(b)) {
        Quat p = to_quat(a), q = to_quat(b);
        double d = p.w * q.w + p.x * q.x + p.y * q.y + p.z * q.z;
        if (d < 0) { q = {-q.w, -q.x, -q.y, -q.z}; d = -d; }
        d = std::min(d, 1.0);
        const double angle = std::acos(d);
        if (2 * angle > MaxRotationRadians) return false;
        double wa = 1 - t, wb = t;
        if (angle > 1e-4) { wa = std::sin((1 - t) * angle) / std::sin(angle); wb = std::sin(t * angle) / std::sin(angle); }
        const Quat r = {wa * p.w + wb * q.w, wa * p.x + wb * q.x, wa * p.y + wb * q.y, wa * p.z + wb * q.z};
        const double w = r.w, x = r.x, y = r.y, z = r.z;
        const double m[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                             2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                             2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)};
        for (int i = 0; i < 9; ++i) out.r[i] = fixed(m[i]);
        return true;
    }
    for (int i = 0; i < 9; ++i) {
        if (std::abs(int(a.r[i]) - int(b.r[i])) > 4096) return false;
        out.r[i] = int16_t(std::lround(a.r[i] + (b.r[i] - a.r[i]) * t));
    }
    return true;
}
double distance(const int32_t* a, const int32_t* b) {
    double s = 0;
    for (int i = 0; i < 3; ++i) { const double d = double(b[i]) - a[i]; s += d * d; }
    return std::sqrt(s);
}
void blend_translation(const int32_t* a, const int32_t* b, double t, int32_t* out) {
    for (int i = 0; i < 3; ++i) out[i] = int32_t(std::llround(a[i] + (double(b[i]) - a[i]) * t));
}

struct Object {
    uint32_t ptr, fn, mesh, flags;
    uint8_t type;
    Matrix m;
};
struct Particle { uint32_t ptr; int32_t pos[3]; };
struct Snapshot {
    bool valid = false;
    uint32_t tick = 0, vblank = 0, camera_node = 0;
    Matrix camera{};
    uint16_t focal = 0;
    std::vector<Object> objects;
    std::vector<Particle> particles;
};

Snapshot snapshots[2];
unsigned current = 0;
uint32_t vblanks = 0;
bool crossfade = false;

// What frame k blends from: matched by pointer AND identity, since objects are
// heap-allocated and a freed address can be reused by an unrelated object.
struct Pair { uint32_t ptr; Matrix from, to; bool rotation; };
struct PassWork {
    std::vector<Pair> objects;
    std::vector<std::pair<uint32_t, std::pair<std::array<int32_t,3>, std::array<int32_t,3>>>> particles;
    Matrix camera_from{}, camera_to{};
    double focal_from = 0, focal_to = 0;
    bool camera_blend = false;
    uint32_t buffer = 0;
    bool split = false;
    unsigned slot = 0;
    unsigned camera = 0;
} work;

void walk(Snapshot& s, uint32_t obj, unsigned depth) {
    while (ram(obj) && s.objects.size() < MaxObjects) {
        Object o;
        o.ptr = obj; o.fn = rd(obj); o.flags = rd(obj + 4); o.type = rb(obj + 8);
        o.mesh = rd(obj + 0x40); o.m = read_matrix(obj + 0x20);
        s.objects.push_back(o);
        const uint32_t child = rd(obj + 0x10);
        if (depth < MaxDepth && ram(child)) walk(s, child, depth + 1);
        if (depth == 0) return;     // a list entry is one root; siblings belong to a parent
        obj = rd(obj + 0x0C);
    }
}
void walk_list(Snapshot& s, uint32_t header) {
    uint32_t node = rd(header);
    for (unsigned guard = 0; ram(node) && guard < 0x800; ++guard) {
        const uint32_t next = rd(node);
        if (!next) break;                   // the last node is the sentinel
        walk(s, rd(node + 8), 0);
        node = next;
    }
}

uint32_t camera_node(uint32_t actor) {
    if (!ram(actor)) return 0;
    return rb(actor + 0xDD) == 2 ? rd(actor + 0x100) : rd(actor + 0xEC);
}

void capture(Snapshot& s, const CPUState* cpu) {
    s.objects.clear(); s.particles.clear();
    s.tick = rd(GP + 0x28);
    s.vblank = vblanks;
    s.camera_node = camera_node(rd(cpu->gpr[30] - ActorTableOffset));
    walk_list(s, GP + 0x1040);
    walk_list(s, GP + 0x10B8);
    uint32_t p = rd(GP + 0x1188);
    for (unsigned guard = 0; ram(p) && guard < MaxParticles; ++guard) {
        Particle q; q.ptr = p;
        for (int i = 0; i < 3; ++i) q.pos[i] = int32_t(rd(p + 0x0C + 4 * i));
        s.particles.push_back(q);
        p = rd(p);
    }
    s.valid = true;
}

void log2_bucket(const char* prefix, double v) {
    static char names[3][40][48];
    const int b = v < 1.0 ? 0 : std::min(39, int(std::floor(std::log2(v))) + 1);
    const int which = prefix[7] == 'c' ? 0 : prefix[7] == 'v' ? 1 : 2;
    char* n = names[which][b];
    if (!n[0]) std::snprintf(n, sizeof names[0][0], "%s%02d", prefix, b);
    psx_mod_counter_add(n, 1);
}

bool prepare(const Snapshot& from, const Snapshot& to, uint32_t ticks) {
    const double teleport = TeleportPerTick * ticks, camera_cut = CameraCutPerTick * ticks;
    work.objects.clear(); work.particles.clear();
    std::unordered_map<uint32_t, const Object*> old;
    old.reserve(from.objects.size() * 2);
    for (const auto& o : from.objects) old.emplace(o.ptr, &o);
    unsigned snapped = 0;
    for (const auto& o : to.objects) {
        const auto f = old.find(o.ptr);
        if (f == old.end()) continue;
        const Object& a = *f->second;
        if (a.fn != o.fn || a.type != o.type || a.mesh != o.mesh) continue;
        if (o.type == 2) log2_bucket("v82.fr.vehicle_move_log2_", distance(a.m.t, o.m.t));
        else log2_bucket("v82.fr.other_move_log2_", distance(a.m.t, o.m.t));
        if (distance(a.m.t, o.m.t) > teleport) {
            ++snapped;
            psx_mod_counter_add(o.type == 2 ? "v82.fr.snap_vehicle" : "v82.fr.snap_other", 1);
            continue;
        }
        // Billboards (flags 0x10) rebuild their rotation from the camera.
        work.objects.push_back({o.ptr, a.m, o.m, (o.flags & 0x10) == 0});
    }
    std::unordered_map<uint32_t, const Particle*> oldp;
    oldp.reserve(from.particles.size() * 2);
    for (const auto& p : from.particles) oldp.emplace(p.ptr, &p);
    for (const auto& p : to.particles) {
        const auto f = oldp.find(p.ptr);
        if (f == oldp.end() || distance(f->second->pos, p.pos) > teleport) continue;
        work.particles.push_back({p.ptr, {{f->second->pos[0], f->second->pos[1], f->second->pos[2]},
                                          {p.pos[0], p.pos[1], p.pos[2]}}});
    }
    work.camera_from = from.camera; work.camera_to = to.camera;
    work.focal_from = from.focal; work.focal_to = to.focal;
    Matrix probe;
    const double cam_move = distance(from.camera.t, to.camera.t);
    const bool same_node = from.camera_node == to.camera_node;
    const bool near = cam_move <= camera_cut;
    const bool turn = blend_rotation(from.camera, to.camera, 0.5, probe);
    work.camera_blend = same_node && near && turn;
    if (!same_node) psx_mod_counter_add("v82.fr.cut_node", 1);
    if (!near) psx_mod_counter_add("v82.fr.cut_distance", 1);
    if (!turn) psx_mod_counter_add(orthonormal(from.camera) && orthonormal(to.camera)
                                   ? "v82.fr.cut_rotation" : "v82.fr.cut_rotation_nonortho", 1);
    log2_bucket("v82.fr.cam_move_log2_", cam_move);
    psx_mod_counter_add("v82.fr.objects_blended", work.objects.size());
    psx_mod_counter_add("v82.fr.objects_snapped", snapped);
    if (!work.camera_blend) psx_mod_counter_add("v82.fr.camera_cuts", 1);
    return work.camera_blend || !work.objects.empty();
}

uint32_t call(CPUState* cpu, uint32_t fn, uint32_t a0 = 0, uint32_t a1 = 0) {
    const CPUState saved = *cpu;
    cpu->pc = 0;
    cpu->gpr[4] = a0; cpu->gpr[5] = a1;
    psx_dispatch_call(cpu, fn, saved.gpr[31]);
    const uint32_t result = cpu->gpr[2];
    *cpu = saved;
    return result;
}

// Registers at SectionStart in the real frame, and the camera it set.
struct SectionEntry {
    bool valid = false;
    uint32_t gpr[32], hi, lo, gte_data[32], gte_ctrl[32];
} section;
SectionEntry split_section[2];
Snapshot split_snapshots[2][2];
Matrix split_camera[2];
uint16_t split_focal[2];
uint32_t split_node[2];
bool split_camera_seen[2] = {};
unsigned split_current = 0, camera_slot = 0;
bool split_pending = false;
uint32_t split_ticks = 0;
bool camera_seen = false;
Matrix frame_camera{};
uint16_t frame_focal = 0;
double pass_alpha = 0;
bool in_pass = false;

void on_section_start(CPUState* cpu, uint32_t) {
    if (g_psx_render_pass_active) return;
    std::memcpy(section.gpr, cpu->gpr, sizeof section.gpr);
    section.hi = cpu->hi; section.lo = cpu->lo;
    std::memcpy(section.gte_data, cpu->gte_data, sizeof section.gte_data);
    std::memcpy(section.gte_ctrl, cpu->gte_ctrl, sizeof section.gte_ctrl);
    section.valid = true;
    camera_seen = false;
}

void on_set_camera(CPUState* cpu, uint32_t) {
    const uint32_t m = cpu->gpr[4];
    if (!ram(m)) return;
    if (!g_psx_render_pass_active) {
        if (split_section[camera_slot].valid) {
            split_camera[camera_slot] = read_matrix(m);
            split_focal[camera_slot] = uint16_t(cpu->gpr[5]);
            split_camera_seen[camera_slot] = true;
        }
        if (section.valid) { frame_camera = read_matrix(m); frame_focal = uint16_t(cpu->gpr[5]); camera_seen = true; }
        return;
    }
    if (!in_pass || !work.camera_blend) return;
    Matrix cam = work.camera_to;
    blend_rotation(work.camera_from, work.camera_to, pass_alpha, cam);
    blend_translation(work.camera_from.t, work.camera_to.t, pass_alpha, cam.t);
    write_matrix(m, cam, true);
    cpu->gpr[5] = uint32_t(std::lround(work.focal_from + (work.focal_to - work.focal_from) * pass_alpha));
}

int pass(CPUState* cpu, void*, uint32_t alpha_q16) {
    const double t = alpha_q16 / 65536.0;
    for (const auto& p : work.objects) {
        Matrix m = p.to;
        if (p.rotation && !blend_rotation(p.from, p.to, t, m)) m = p.to;
        blend_translation(p.from.t, p.to.t, t, m.t);
        write_matrix(p.ptr + 0x20, m, p.rotation);
    }
    for (const auto& p : work.particles) {
        int32_t pos[3];
        blend_translation(p.second.first.data(), p.second.second.data(), t, pos);
        for (int i = 0; i < 3; ++i) wr(p.first + 0x0C + 4 * i, uint32_t(pos[i]));
    }
    wr(GP + 0x614, 1);            // the previous flip is done, so 0x80014D94 returns
    if (work.split) wr(GP + 0x24, work.buffer);
    std::memcpy(cpu->gpr, section.gpr, sizeof section.gpr);
    cpu->hi = section.hi; cpu->lo = section.lo;
    std::memcpy(cpu->gte_data, section.gte_data, sizeof section.gte_data);
    std::memcpy(cpu->gte_ctrl, section.gte_ctrl, sizeof section.gte_ctrl);
    pass_alpha = t;
    in_pass = true;
    const int ok = psx_mod_run_guest_span(cpu, work.split ? SplitStart[work.camera] : SectionStart,
                                             work.split ? SplitEnd[work.camera] : SectionEnd);
    in_pass = false;
    if (!ok) { psx_mod_counter_add("v82.fr.span_failed", 1); return 0; }
    // Submit as the screen gets it: world OT (the VBlank callback's DrawOTag,
    // here behind the DR_ENV of world(k)'s buffer), then the HUD list.
    // Vertical 2P draws the left camera first (6/7), then the right (8/9).
    const uint32_t env = DrawEnvs + 0x5C * (work.buffer + (work.split ? (work.slot ? 8 : 6) : 0)), dr = env + 0x1C;
    const uint32_t tail = rd(GP + 0xCE0) + 0x3FFC, hud = section.gpr[29] + 0x68;
    call(cpu, SetDrawEnv, dr, env);
    const uint32_t link = rd(tail);
    wr(tail, dr & 0xFFFFFF);
    wr(dr, (uint32_t(rb(env + 0x1F)) << 24) | link);
    call(cpu, DrawOTag, tail);
    call(cpu, DrawOTag, hud);
    call(cpu, DrawSync, 0);
    return 1;
}

// While passes cannot run for a lasting reason, a crossfade is better than
// stock-rate frames; switch back to HOLD once they can.
void update_fallback(uint32_t status) {
    const bool lasting = status == PSX_MOD_RENDER_PASS_BACKEND || status == PSX_MOD_RENDER_PASS_DISABLED;
    if (lasting != crossfade) {
        crossfade = lasting;
        psx_mod_set_frame_interpolation_blend(lasting ? PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE
                                                      : PSX_MOD_FRAME_INTERPOLATION_HOLD);
    }
}

void on_wait_flip(CPUState* cpu, uint32_t) {
    if (g_psx_render_pass_active) return;
    Snapshot& now = snapshots[current];
    Snapshot& before = snapshots[current ^ 1];
    // The 1P full-screen section ran this frame from its start, camera
    // included; anything else (2P views, menus) shows stock frames.
    const bool replayable = section.valid && camera_seen && cpu->gpr[31] == WaitFlipReturn1P;
    section.valid = false;
    if (!replayable) { psx_mod_counter_add("v82.fr.skip_not_1p", 1); now.valid = before.valid = false; return; }
    capture(now, cpu);
    now.camera = frame_camera;
    now.focal = frame_focal;
    current ^= 1;
    psx_mod_counter_add("v82.fr.frames", 1);

    const uint32_t status = psx_mod_render_pass_status();
    update_fallback(status);
    if (!before.valid) { psx_mod_counter_add("v82.fr.skip_no_history", 1); return; }
    // world(k) is drawn by the VBlank callback that flipped to world(k-1);
    // until that has run there is nothing to show yet.
    if (rd(GP + 0x614) == 0) { psx_mod_counter_add("v82.fr.skip_world_pending", 1); return; }
    const uint32_t ticks = now.tick - before.tick;
    if (ticks == 0 || ticks > 8 || now.vblank - before.vblank > 8) { psx_mod_counter_add("v82.fr.skip_ticks", 1); return; }
    // world(k)'s buffer, shown by the flip after this frame's submit
    // (DISPENV[gp+0x24]); the flip done already showed DISPENV[buffer].
    const uint32_t next = rd(GP + 0x24), buffer = 1 - next;
    const uint32_t disp = DispEnvs + 20 * next;
    if (next > 1 || rd(GP + 0xCD0) != DispEnvs + 20 * buffer) { psx_mod_counter_add("v82.fr.buffer_mismatch", 1); return; }
    if (!prepare(before, now, ticks)) { psx_mod_counter_add("v82.fr.skip_nothing", 1); return; }
    work.split = false;
    psx_mod_set_render_pass_flip(PSX_MOD_RENDER_PASS_FLIP_PENDING);

    uint32_t phases[MaxPhases];
    const uint32_t n = psx_mod_render_pass_plan(ticks, 0, phases, MaxPhases);
    if (!n) { psx_mod_counter_add("v82.fr.skip_plan_empty", 1); return; }
    work.buffer = buffer;
    PSXModRenderPass rect{};
    rect.struct_size = sizeof rect;
    rect.x = rh(disp); rect.y = rh(disp + 2); rect.w = rh(disp + 4); rect.h = rh(disp + 6);
    for (uint32_t i = 0; i < n; ++i) {
        rect.alpha_q16 = phases[i];
        if (psx_mod_render_pass(cpu, &rect, pass, nullptr)) psx_mod_counter_add("v82.fr.passes", 1);
        else psx_mod_counter_add("v82.fr.pass_refused", 1);
    }
}

void render_split_pending(CPUState* cpu) {
    if (!split_pending) return;
    split_pending = false;
    psx_mod_set_render_pass_netplay(1);
    psx_mod_set_render_pass_flip(PSX_MOD_RENDER_PASS_FLIP_PENDING);
    const uint32_t status = psx_mod_render_pass_status();
    if (status != PSX_MOD_RENDER_PASS_READY) {
        psx_mod_counter_add("v82.fr.netplay_not_ready", 1); return;
    }
    const uint32_t disp = DispEnvs + 20 * (1 - work.buffer);
    PSXModRenderPass rect{};
    rect.struct_size = sizeof rect;
    rect.x = rh(disp) + 160 * work.slot; rect.y = rh(disp + 2); rect.w = 160; rect.h = rh(disp + 6);
    uint32_t phases[MaxPhases];
    const uint32_t n = psx_mod_render_pass_plan(split_ticks, 0, phases, MaxPhases);
    for (uint32_t i = 0; i < n; ++i) {
        rect.alpha_q16 = phases[i];
        if (psx_mod_render_pass(cpu, &rect, pass, nullptr)) psx_mod_counter_add("v82.fr.netplay_passes", 1);
        else psx_mod_counter_add("v82.fr.netplay_pass_refused", 1);
    }
}

void on_split_start(CPUState* cpu, uint32_t pc) {
    if (g_psx_render_pass_active || psx_netplay_is_resimulating()) return;
    camera_slot = pc == SplitStart[1] ? 1 : 0;
    if (!camera_slot) {
        split_section[0].valid = split_section[1].valid = false;
        split_camera_seen[0] = split_camera_seen[1] = false;
    }
    SectionEntry& s = split_section[camera_slot];
    std::memcpy(s.gpr, cpu->gpr, sizeof s.gpr);
    s.hi = cpu->hi; s.lo = cpu->lo;
    std::memcpy(s.gte_data, cpu->gte_data, sizeof s.gte_data);
    std::memcpy(s.gte_ctrl, cpu->gte_ctrl, sizeof s.gte_ctrl);
    s.valid = true;
    split_node[camera_slot] = camera_node(rd(camera_slot ? cpu->gpr[21] + 4 : cpu->gpr[30] - ActorTableOffset));
}

void on_split_synced(CPUState* cpu, uint32_t) {
    // The native DrawSync has finished the previous camera's asynchronous
    // world DMA. Its framebuffer remains intact until the next native submit.
    if (!g_psx_render_pass_active && !psx_netplay_is_resimulating()) render_split_pending(cpu);
}

void on_split_done(CPUState* cpu, uint32_t) {
    if (g_psx_render_pass_active || !psx_netplay_active()) return;
    if (psx_netplay_is_resimulating() || rd(GP + 0x2C) != 2 ||
        !split_section[0].valid || !split_section[1].valid ||
        !split_camera_seen[0] || !split_camera_seen[1]) {
        for (auto& pair : split_snapshots) for (auto& s : pair) s.valid = false;
        split_pending = false;
        return;
    }
    const unsigned slot = psx_netplay_local_port() == 1 ? 1 : 0, camera = slot;
    Snapshot& now = split_snapshots[camera][split_current];
    Snapshot& before = split_snapshots[camera][split_current ^ 1];
    capture(now, cpu);
    now.camera = split_camera[camera]; now.focal = split_focal[camera]; now.camera_node = split_node[camera];
    split_current ^= 1;
    psx_mod_counter_add("v82.fr.netplay_frames", 1);
    const uint32_t ticks = now.tick - before.tick;
    if (!before.valid || ticks == 0 || ticks > 8 || now.vblank - before.vblank > 8) return;
    if (!prepare(before, now, ticks)) return;
    section = split_section[camera];
    work.split = true; work.slot = slot; work.camera = camera; work.buffer = rd(GP + 0x24);
    if (work.buffer > 1) return;
    split_ticks = ticks; split_pending = true;
}

void on_vblank() { ++vblanks; }

void activate() {
    char rate[16];
    unsigned long fps = 0;                 // 0 follows the measured display refresh
    if (psx_mod_option_value(Package, Feature, "rate", rate, sizeof rate) && std::strcmp(rate, "display") != 0) {
        char* end = rate;
        const unsigned long parsed = std::strtoul(rate, &end, 10);
        if (end != rate && *end == '\0') fps = parsed;
    }
    snapshots[0].valid = snapshots[1].valid = false;
    section.valid = camera_seen = false;
    split_section[0].valid = split_section[1].valid = false;
    split_pending = false;
    for (auto& pair : split_snapshots) for (auto& s : pair) s.valid = false;
    crossfade = false;
    psx_mod_set_frame_interpolation_source(PSX_MOD_FRAME_SOURCE_FLIP);
    psx_mod_set_frame_interpolation_blend(PSX_MOD_FRAME_INTERPOLATION_HOLD);
    psx_mod_set_frame_interpolation(uint32_t(fps));
    psx_mod_set_render_pass_netplay(1);
}
} // namespace

PSX_MOD_CONSTRUCTOR(v82_register_frame_rate_plugin) {
    psx_mod_register_activation_plugin(Plugin, activate);
    psx_mod_register_vblank_plugin(Plugin, on_vblank);
    psx_mod_register_function_entry_plugin(Plugin, WaitFlip, on_wait_flip);
    psx_mod_register_function_entry_plugin(Plugin, SetCamera, on_set_camera);
    psx_mod_register_instruction_plugin(Plugin, SectionStart, SectionStartWord, on_section_start);
    psx_mod_register_instruction_plugin(Plugin, SplitStart[0], 0x0C0052CF, on_split_start);
    psx_mod_register_instruction_plugin(Plugin, SplitStart[1], 0x0C0052CF, on_split_start);
    psx_mod_register_instruction_plugin(Plugin, SplitDone, 0x080051CF, on_split_done);
    psx_mod_register_instruction_plugin(Plugin, SplitSynced, 0x3C048007, on_split_synced);
}
