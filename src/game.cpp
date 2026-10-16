#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static const Color INK{245, 235, 210}, DIM{170, 160, 140}, PANEL{40, 30, 22, 235}, EDGE{200, 160, 80};
static const char* BRANCHES[3] = {"hunters", "economy", "spirits"};
static const char* BRANCH_RU[3] = {"Охотники", "Хозяйство", "Духи"};

static std::string resName(const std::string& r) {
    if (r == "wood") return "Дерево";
    if (r == "stone") return "Камень";
    if (r == "food") return "Еда";
    return r;
}

bool Game::init(const std::string& dir) {
    std::string err;
    if (!data.load(dir, err)) { fprintf(stderr, "data: %s\n", err.c_str()); return false; }
    srand(7);
    for (int x = 0; x < MW; x++) for (int y = 0; y < MH; y++) {
        occ[x][y] = -1; int r = rand() % 100;
        map[x][y] = r < 14 ? FOREST : r < 20 ? ROCK : GRASS;
        if (x < 2 || y < 2 || x > MW - 3 || y > MH - 3) map[x][y] = (rand() % 3) ? WATER : GRASS;
    }
    for (int x = 17; x < 24; x++) for (int y = 12; y < 19; y++) map[x][y] = GRASS;
    addBuilding(*data.building("chief_hut"), 19, 14);
    for (int i = 0; i < 4; i++) spawnVillager();
    return true;
}
std::string Game::stats() const {
    std::string o = "t=" + std::to_string((int)gameTime) + " pop=" + std::to_string(villagers.size());
    o += " warriors=" + std::to_string(soldierCount(0)) + " enemies=" + std::to_string(soldierCount(1));
    for (auto& s : soldiers) o += "\n  " + s.def->id + " f" + std::to_string(s.faction) + " hp=" + std::to_string((int)s.hp) + " at(" + std::to_string((int)(s.x / TS)) + "," + std::to_string((int)(s.y / TS)) + ")";
    for (auto& b : buildings) if (!b.alive || b.hp < b.def->hp) o += "\n  bld " + b.def->id + " hp=" + std::to_string((int)b.hp) + (b.alive ? "" : " (руины)");
    o += "\n  camp hp=" + std::to_string((int)camp.hp) + (camp.captured ? " (взято)" : "") + " chapter=" + std::to_string(chapter) + " attack=" + (attackOrder ? "1" : "0");
    return o;
}
void Game::debugUnlock() {
    for (auto& kv : res) kv.second = 500;
    era = 5; skillPoints = 10;
    for (auto& s : data.skills) learned.insert(s.id);
    say("Режим отладки");
}

int Game::addBuilding(const BuildingDef& b, int x, int y) {
    Placed p; p.def = &b; p.x = x; p.y = y; p.hp = (float)b.hp;
    buildings.push_back(p);
    int idx = (int)buildings.size() - 1;
    for (int dx = 0; dx < b.w; dx++) for (int dy = 0; dy < b.h; dy++) occ[x + dx][y + dy] = idx;
    return idx;
}
int Game::buildingAt(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return -1;
    int i = occ[tx][ty];
    if (i < 0 || !buildings[i].alive) return -1;
    return i;
}
bool Game::hasBuilding(const std::string& id) const { for (auto& b : buildings) if (b.alive && b.def->id == id) return true; return false; }
int Game::housing() const {
    int h = 0; for (auto& b : buildings) if (b.alive) h += b.def->housing;
    return std::min(h, data.eras[era - 1].pop);
}
void Game::spawnVillager() { Villager v; v.x = 20.5f * TS; v.y = 16.5f * TS; villagers.push_back(v); pop = (int)villagers.size(); }
void Game::spawnSoldier(const UnitDef& u, float x, float y, int faction) {
    Soldier s; s.def = &u; s.x = x; s.y = y; s.hp = (float)u.hp; s.faction = faction;
    // точка сбора: у хижины вождя, бойцы распределяются по кругу
    const Placed& c = buildings[0];
    int k = soldierCount(0);
    s.gx = (c.x + 1.5f) * TS + (k % 5 - 2) * 14.0f;
    s.gy = (c.y + 2.5f) * TS + (k / 5) * 14.0f + 10;
    soldiers.push_back(s);
}
int Game::soldierCount(int faction) const { int n = 0; for (auto& s : soldiers) if (s.faction == faction) n++; return n; }

std::vector<const BuildingDef*> Game::buildable() const {
    std::vector<const BuildingDef*> o;
    for (auto& b : data.buildings) {
        if (b.era > era) continue;
        if (!b.needsSkill.empty() && !learned.count(b.needsSkill)) continue;
        o.push_back(&b);
    }
    return o;
}
bool Game::canAfford(const std::map<std::string, int>& cost) const {
    for (auto& kv : cost) { auto it = res.find(kv.first); if (it == res.end() || it->second < kv.second) return false; }
    return true;
}
void Game::pay(const std::map<std::string, int>& cost) { for (auto& kv : cost) res[kv.first] -= kv.second; }

bool Game::tryPlace(const BuildingDef& b) {
    if (cx + b.w > MW || cy + b.h > MH) { say("Нет места"); return false; }
    for (int dx = 0; dx < b.w; dx++) for (int dy = 0; dy < b.h; dy++)
        if (occ[cx + dx][cy + dy] >= 0 || map[cx + dx][cy + dy] != GRASS) { say("Место занято"); return false; }
    if (!canAfford(b.cost)) { say("Не хватает ресурсов"); return false; }
    pay(b.cost);
    addBuilding(b, cx, cy);
    say(b.name + ": построено"); return true;
}
bool Game::tryTrain() {
    int bi = buildingAt(cx, cy);
    if (bi < 0 || buildings[bi].def->trains.empty()) { say("Встаньте на здание, где кого-то обучают"); return false; }
    const Placed& p = buildings[bi];
    // берём первого юнита из списка, которого уже можно обучить
    const UnitDef* u = nullptr;
    for (auto& id : p.def->trains) {
        const UnitDef* c = data.unit(id);
        if (c && (c->needsSkill.empty() || learned.count(c->needsSkill))) { u = c; break; }
    }
    if (!u) {
        const UnitDef* c = data.unit(p.def->trains[0]);
        const SkillDef* s = c ? data.skill(c->needsSkill) : nullptr;
        say("Сначала изучите: " + (s ? s->name : std::string("умение")));
        return false;
    }
    if (!canAfford(u->cost)) { say("Не хватает ресурсов"); return false; }
    pay(u->cost);
    int k = soldierCount(0);
    spawnSoldier(*u, (p.x + p.def->w / 2.0f) * TS + (k % 4) * 10 - 15, (p.y + p.def->h) * TS + 6 + (k / 4) * 12, 0);
    say(u->name + " обучен"); return true;
}
const SkillDef* Game::skillAt(int col, int row) const {
    for (auto& s : data.skills) if (s.branch == BRANCHES[col] && s.tier == row + 1) return &s;
    return nullptr;
}
bool Game::canLearn(const SkillDef& s) const {
    if (learned.count(s.id) || skillPoints < s.cost) return false;
    for (auto& r : s.requires) if (!learned.count(r)) return false;
    return true;
}
bool Game::tryAdvanceEra() {
    if (era >= (int)data.eras.size()) { say("Высшая эпоха"); return false; }
    int next = era + 1; bool ok = false; std::string why;
    switch (next) {
        case 2: ok = pop >= 6 && hasBuilding("kitchen"); why = "Нужно 6 жителей и кухня"; break;
        case 3: ok = spentPoints >= 2 && hasBuilding("armoury"); why = "Нужны 2 потраченных очка умений и оружейная"; break;
        case 4: ok = hasBuilding("forge") && pop >= 15; why = "Нужны кузница и 15 жителей"; break;
        case 5: ok = learned.count("siege_craft") && hasBuilding("wall"); why = "Нужны осадное дело и стены"; break;
        default: break;
    }
    if (!ok) { say(why); return false; }
    era = next; skillPoints += data.pointsPerEra;
    say("Эпоха: " + data.eras[era - 1].name + "! +" + std::to_string(data.pointsPerEra) + " очка умений");
    return true;
}

void Game::updateVillagers(float dt) {
    for (auto& v : villagers) {
        if (v.state == 0) {
            float best = 1e9; int bx = -1, by = -1;
            for (int x = 0; x < MW; x++) for (int y = 0; y < MH; y++) {
                if (map[x][y] != FOREST && map[x][y] != ROCK) continue;
                float d = std::hypot(x * TS - v.x, y * TS - v.y) + (rand() % 40);
                if (d < best) { best = d; bx = x; by = y; }
            }
            if (bx >= 0) { v.tx = bx; v.ty = by; v.state = 1; }
        } else if (v.state == 1 || v.state == 3) {
            float tx = v.state == 1 ? v.tx * TS + TS / 2 : 20.5f * TS, ty = v.state == 1 ? v.ty * TS + TS / 2 : 16.5f * TS;
            float dx = tx - v.x, dy = ty - v.y, d = std::hypot(dx, dy);
            if (d < 4) {
                if (v.state == 1) { v.state = 2; v.timer = 3; v.carryType = map[v.tx][v.ty] == FOREST ? 0 : 1; }
                else { res[v.carryType == 0 ? "wood" : "stone"] += v.carry; v.carry = 0; v.state = 0; }
            } else { v.x += dx / d * 36 * dt; v.y += dy / d * 36 * dt; }
        } else if (v.state == 2) { v.timer -= dt; if (v.timer <= 0) { v.carry = 5; v.state = 3; } }
    }
}

void Game::moveTo(Soldier& s, float tx, float ty, float dt, float stop) {
    float dx = tx - s.x, dy = ty - s.y, d = std::hypot(dx, dy);
    if (d <= stop || d < 0.001f) return;
    float step = std::min(d - stop, s.def->speed * dt);
    s.x += dx / d * step; s.y += dy / d * step;
}
void Game::damage(int kind, int idx, float amount) {
    if (kind == 0) soldiers[idx].hp -= amount;
    else if (kind == 1) villagers[idx].hp -= amount;
    else if (kind == 2) buildings[idx].hp -= amount;
    else if (kind == 3) camp.hp -= amount;
}
void Game::captureCamp() {
    camp.captured = true; attackOrder = false;
    if (const BuildingDef* h = data.building("hut")) addBuilding(*h, camp.x, camp.y);
    chapter = 2;
    say("Стойбище взято! Глава 2");
}
void Game::cleanup() {
    soldiers.erase(std::remove_if(soldiers.begin(), soldiers.end(), [](const Soldier& s) { return s.hp <= 0; }), soldiers.end());
    villagers.erase(std::remove_if(villagers.begin(), villagers.end(), [](const Villager& v) { return v.hp <= 0; }), villagers.end());
    pop = (int)villagers.size();
    for (auto& b : buildings) if (b.alive && b.hp <= 0) {
        b.alive = false;
        if (b.def->id == "chief_hut") { mode = Mode::Over; say("Вождь пал"); }
        else say(b.def->name + " разрушено");
    }
}

void Game::spawnWave(const WaveDef& w) {
    bool right = rand() % 2;
    float baseX = right ? (MW - 1) * TS : 0.0f;
    float baseY = (6 + rand() % (MH - 12)) * TS;
    int i = 0;
    for (auto& kv : w.units) {
        const UnitDef* u = data.unit(kv.first);
        if (!u) continue;
        for (int n = 0; n < kv.second; n++, i++) {
            float ox = (i % 4) * 14.0f * (right ? -1 : 1), oy = (i / 4) * 16.0f;
            spawnSoldier(*u, baseX + ox, baseY + oy, 1);
        }
    }
    wavesSpawned++;
    say("Нападение! Волна " + std::to_string(wavesSpawned));
}

void Game::updateSoldiers(float dt) {
    const float aggro = 9.0f * TS, leash = 16.0f * TS;
    for (size_t k = 0; k < soldiers.size(); k++) {
        Soldier& s = soldiers[k];
        const UnitDef& u = *s.def;
        s.cd -= dt;
        float reach = (attackOrder && s.faction == 0) ? 1e9f : std::max(aggro, u.range * TS);
        struct Tgt { int kind = -1, idx = -1; float d = 1e9f, half = 0, x = 0, y = 0; } best;
        // расстояние до края цели; kind: 0 — боец, 1 — житель, 2 — здание
        auto pick = [&](int kind, int idx, float x, float y, float half) {
            float d = std::hypot(s.x - x, s.y - y) - half;
            if (d <= reach && d < best.d) best = Tgt{kind, idx, d, half, x, y};
        };
        if (s.faction == 1) {
            for (size_t j = 0; j < soldiers.size(); j++) if (soldiers[j].faction == 0) pick(0, (int)j, soldiers[j].x, soldiers[j].y, 0);
            for (size_t j = 0; j < villagers.size(); j++) pick(1, (int)j, villagers[j].x, villagers[j].y, 0);
            for (size_t i = 0; i < buildings.size(); i++) if (buildings[i].alive) {
                const Placed& p = buildings[i];
                pick(2, (int)i, (p.x + p.def->w / 2.0f) * TS, (p.y + p.def->h / 2.0f) * TS, std::max(p.def->w, p.def->h) * TS / 2.0f);
            }
        } else {
            for (size_t j = 0; j < soldiers.size(); j++) if (soldiers[j].faction == 1) pick(0, (int)j, soldiers[j].x, soldiers[j].y, 0);
            if (attackOrder && !camp.captured && camp.hp > 0)
                pick(3, 0, (camp.x + 1.0f) * TS, (camp.y + 1.0f) * TS, 1.5f * TS);
        }
        // свой боец далеко от точки сбора — возвращается, даже если видит врага (при атаке не возвращается)
        bool leashed = s.faction == 0 && !attackOrder && std::hypot(s.x - s.gx, s.y - s.gy) > leash;
        if (best.kind < 0 || leashed) {
            if (s.faction == 1) { const Placed& c = buildings[0]; moveTo(s, (c.x + 1.5f) * TS, (c.y + 1) * TS, dt, 2.0f * TS); }
            else moveTo(s, s.gx, s.gy, dt, 4.0f);
            continue;
        }
        if (best.d > u.range * TS) { moveTo(s, best.x, best.y, dt, u.range * TS + best.half); continue; }
        if (best.d < u.minRange * TS) continue;
        if (s.cd > 0) continue;
        s.cd = u.reload;
        float dmg = u.dmg * ((best.kind == 2 || best.kind == 3) ? u.vsBuildings : 1.0f);
        int tkind = best.kind, tidx = best.idx;
        float ix = best.x, iy = best.y;
        damage(tkind, tidx, dmg);
        if (u.splash > 0) {
            float r = u.splash * TS;
            for (size_t j = 0; j < soldiers.size(); j++) {
                if (soldiers[j].faction == s.faction) continue;
                if (tkind == 0 && (int)j == tidx) continue;
                if (std::hypot(soldiers[j].x - ix, soldiers[j].y - iy) <= r) soldiers[j].hp -= dmg * 0.5f;
            }
        }
        fxs.push_back({ix, iy, 0.35f, u.splash > 0 ? u.splash * TS : 6.0f,
                       u.splash > 0 ? Color{230, 140, 60} : Color{255, 240, 160}});
    }
    if (!camp.captured && camp.hp <= 0) captureCamp();
    cleanup();
}

void Game::update(const Input& in, float dt) {
    if (toastT > 0) toastT -= dt;
    for (auto it = fxs.begin(); it != fxs.end();) { it->t -= dt; if (it->t <= 0) it = fxs.erase(it); else ++it; }
    if (mode == Mode::Over) {
        if (in.pressed[SELECT] || in.pressed[START]) quit = true;
    } else if (mode == Mode::Play) {
        if (in.pressed[LEFT]) cx = std::max(0, cx - 1);
        if (in.pressed[RIGHT]) cx = std::min(MW - 1, cx + 1);
        if (in.pressed[UP]) cy = std::max(0, cy - 1);
        if (in.pressed[DOWN]) cy = std::min(MH - 1, cy + 1);
        if (in.pressed[X]) { mode = Mode::Build; menuSel = 0; }
        if (in.pressed[Y]) { mode = Mode::Skills; skillCol = skillRow = 0; }
        if (in.pressed[SELECT]) tryAdvanceEra();
        if (in.pressed[START]) mode = Mode::Pause;
        if (in.pressed[A]) tryTrain();
        if (in.pressed[L1] && era >= 3 && !camp.captured) {
            attackOrder = !attackOrder;
            say(attackOrder ? "Приказ: атаковать стойбище" : "Приказ: вернуться к вождю");
        }
        foodTimer += dt;
        if (foodTimer >= 5) { foodTimer = 0; res["food"] += 2 + (hasBuilding("kitchen") ? 4 : 0) + (learned.count("fire_kitchen") ? 2 : 0); }
        eraTimer += dt;
        if (eraTimer >= 8 && (int)villagers.size() < housing() && res["food"] >= 10) { eraTimer = 0; res["food"] -= 10; spawnVillager(); }
        gameTime += dt;
        while (nextWave < data.waves.size() && gameTime >= data.waves[nextWave].at) { spawnWave(data.waves[nextWave]); nextWave++; }
        updateVillagers(dt);
        updateSoldiers(dt);
    } else if (mode == Mode::Build) {
        auto list = buildable(); int n = (int)list.size();
        if (in.pressed[DOWN]) menuSel = (menuSel + 1) % n;
        if (in.pressed[UP]) menuSel = (menuSel + n - 1) % n;
        if (in.pressed[B] || in.pressed[X]) mode = Mode::Play;
        if (in.pressed[A]) { if (tryPlace(*list[menuSel])) mode = Mode::Play; }
    } else if (mode == Mode::Skills) {
        if (in.pressed[LEFT]) skillCol = (skillCol + 2) % 3;
        if (in.pressed[RIGHT]) skillCol = (skillCol + 1) % 3;
        if (in.pressed[UP]) skillRow = std::max(0, skillRow - 1);
        if (in.pressed[DOWN]) skillRow = std::min(2, skillRow + 1);
        if (in.pressed[B] || in.pressed[Y]) mode = Mode::Play;
        if (in.pressed[A]) {
            if (auto* s = skillAt(skillCol, skillRow)) {
                if (canLearn(*s)) { skillPoints -= s->cost; spentPoints += s->cost; learned.insert(s->id); say("Изучено: " + s->name); }
                else say(learned.count(s->id) ? "Уже изучено" : "Нужны очки умений или предыдущее умение");
            }
        }
    } else if (mode == Mode::Pause) {
        if (in.pressed[START] || in.pressed[B] || in.pressed[A]) mode = Mode::Play;
        if (in.pressed[SELECT]) quit = true;
    }
    camX = std::clamp(cx * TS - Gfx::W / 2 + TS / 2, 0, MW * TS - Gfx::W);
    camY = std::clamp(cy * TS - (Gfx::H - 56) / 2 + TS / 2, 0, MH * TS - (Gfx::H - 56));
}

void Game::drawMap(Gfx& g) {
    for (int x = 0; x < MW; x++) for (int y = 0; y < MH; y++) {
        int sx = x * TS - camX, sy = y * TS - camY + 28;
        if (sx < -TS || sy < -TS || sx > Gfx::W || sy > Gfx::H) continue;
        Color c{96, 150, 70};
        if ((x + y) % 2) c = {90, 144, 66};
        if (map[x][y] == FOREST) c = {40, 100, 50}; else if (map[x][y] == ROCK) c = {130, 130, 125}; else if (map[x][y] == WATER) c = {50, 100, 170};
        g.fill(sx, sy, TS, TS, c);
        if (map[x][y] == FOREST) { g.fill(sx + 12, sy + 16, 8, 12, {90, 60, 30}); g.fill(sx + 6, sy + 4, 20, 14, {30, 80, 40}); }
        if (map[x][y] == ROCK) { g.fill(sx + 6, sy + 12, 20, 14, {105, 105, 100}); g.fill(sx + 10, sy + 8, 12, 8, {150, 150, 145}); }
    }
    for (auto& b : buildings) {
        int sx = b.x * TS - camX, sy = b.y * TS - camY + 28, w = b.def->w * TS, h = b.def->h * TS;
        if (!b.alive) {
            g.fill(sx + 2, sy + 2, w - 4, h - 4, {70, 60, 50});
            g.frame(sx + 2, sy + 2, w - 4, h - 4, {40, 30, 20}, 2);
            g.text("Руины", sx + w / 2, sy + h / 2 - 7, 12, DIM, 1);
            continue;
        }
        auto& k = b.def->color;
        g.fill(sx + 2, sy + 2, w - 4, h - 4, {k[0], k[1], k[2]});
        g.frame(sx + 2, sy + 2, w - 4, h - 4, {40, 30, 20}, 2);
        int nw = g.textWidth(b.def->name, 12); g.text(b.def->name, sx + w / 2, sy + h / 2 - 7, nw > w - 6 ? 9 : 12, {255, 255, 255}, 1);
        if (b.hp < b.def->hp) {
            float f = std::max(0.0f, b.hp) / b.def->hp;
            g.fill(sx + 4, sy + h - 8, w - 8, 4, {60, 0, 0});
            g.fill(sx + 4, sy + h - 8, (int)((w - 8) * f), 4, {120, 220, 90});
        }
    }
    if (!camp.captured) {
        // стойбище людоедов: три шалаша и полоска здоровья (графика собственная, простые фигуры)
        int sx = camp.x * TS - camX, sy = camp.y * TS - camY + 28;
        for (int i = 0; i < 3; i++) {
            int hx = sx + i * 40, hy = sy + (i == 1 ? 0 : 20);
            g.fill(hx + 4, hy + 4, 30, 26, {120, 80, 50});
            g.fill(hx + 12, hy + 14, 14, 16, {50, 30, 20});
        }
        g.fill(sx, sy - 8, 100, 5, {60, 0, 0});
        g.fill(sx, sy - 8, (int)(100 * std::max(0.0f, camp.hp) / camp.maxhp), 5, {220, 80, 60});
        g.text("Стойбище", sx + 50, sy - 22, 12, {255, 220, 200}, 1);
    }
    for (auto& v : villagers) {
        int sx = (int)v.x - camX, sy = (int)v.y - camY + 28;
        g.fill(sx - 4, sy - 8, 8, 12, {230, 190, 140}); g.fill(sx - 5, sy - 10, 10, 4, {80, 50, 30});
        if (v.carry) g.fill(sx + 4, sy - 4, 5, 5, v.carryType == 0 ? Color{140, 90, 40} : Color{170, 170, 170});
    }
    for (auto& s : soldiers) {
        int sx = (int)s.x - camX, sy = (int)s.y - camY + 28;
        if (s.def->id == "catapult") {
            g.fill(sx - 12, sy - 4, 24, 12, {110, 80, 50});
            g.fill(sx - 14, sy + 6, 8, 8, {40, 30, 20}); g.fill(sx + 6, sy + 6, 8, 8, {40, 30, 20});
            g.fill(sx - 2, sy - 16, 4, 14, {90, 60, 30});
        } else {
            Color body = s.faction == 1 ? Color{170, 60, 50} : Color{70, 120, 200};
            g.fill(sx - 4, sy - 8, 8, 12, body);
            g.fill(sx - 5, sy - 10, 10, 4, {60, 40, 25});
            if (s.def->weapon == "spear" || s.def->weapon == "javelin") g.fill(sx + 5, sy - 14, 2, 20, {210, 210, 210});
        }
        float maxhp = (float)s.def->hp;
        if (s.hp < maxhp) {
            g.fill(sx - 10, sy - 20, 20, 3, {60, 0, 0});
            g.fill(sx - 10, sy - 20, (int)(20 * std::max(0.0f, s.hp) / maxhp), 3, {120, 220, 90});
        }
    }
    for (auto& f : fxs) {
        float p = f.t / 0.35f;
        Color c = f.c; c.a = (Uint8)(p * 140);
        g.disc((int)f.x - camX, (int)f.y - camY + 28, (int)(f.r * (1.2f - p * 0.4f)), c);
    }
    int sx = cx * TS - camX, sy = cy * TS - camY + 28;
    g.frame(sx, sy, TS, TS, {255, 240, 120}, 3);
}
void Game::drawHud(Gfx& g) {
    g.fill(0, 0, Gfx::W, 28, {30, 22, 16, 240});
    g.text("Дерево " + std::to_string(res["wood"]), 8, 5, 16, INK);
    g.text("Камень " + std::to_string(res["stone"]), 118, 5, 16, INK);
    g.text("Еда " + std::to_string(res["food"]), 238, 5, 16, INK);
    g.text("Жители " + std::to_string(villagers.size()) + "/" + std::to_string(housing()), 326, 5, 16, INK);
    g.text("Воины " + std::to_string(soldierCount(0)), 460, 5, 16, INK);
    g.text("Эпоха " + std::to_string(era), Gfx::W - 8, 5, 16, EDGE, 2);
    std::string goal = camp.captured ? "Глава " + std::to_string(chapter) + ": стойбище взято"
                     : era < 3 ? "Глава 1: отбить волны и развиваться до 3 эпохи"
                     : attackOrder ? "Глава 1: идёт атака на стойбище людоедов"
                     : "Глава 1: нажмите L1, чтобы атаковать стойбище людоедов";
    g.text(goal, 8, 32, 14, EDGE);
    if (toastT > 0) { int w = g.textWidth(toast, 18) + 24; g.panel(Gfx::W / 2 - w / 2, 48, w, 30, PANEL, EDGE); g.text(toast, Gfx::W / 2, 52, 18, INK, 1); }
}
void Game::drawHints(Gfx& g) {
    g.fill(0, Gfx::H - 28, Gfx::W, 28, {30, 22, 16, 240});
    struct H { const char* b; const char* t; Color c; };
    std::vector<H> hs;
    if (mode == Mode::Play) {
        hs = {{"X", "Строить", {90, 140, 230}}, {"Y", "Умения", {240, 200, 70}}, {"SEL", "Эпоха", {200, 200, 200}}, {"STA", "Пауза", {200, 200, 200}}};
        int bi = buildingAt(cx, cy);
        if (bi >= 0 && !buildings[bi].def->trains.empty()) hs.push_back({"A", "Обучить", {90, 200, 110}});
        if (era >= 3 && !camp.captured) hs.push_back({"L1", attackOrder ? "Отозвать" : "Атака", {230, 120, 60}});
    }
    else if (mode == Mode::Build) hs = {{"A", "Построить", {90, 200, 110}}, {"B", "Назад", {230, 90, 90}}};
    else if (mode == Mode::Skills) hs = {{"A", "Изучить", {90, 200, 110}}, {"B", "Назад", {230, 90, 90}}};
    else if (mode == Mode::Over) hs = {{"SEL", "Выйти", {200, 200, 200}}};
    else hs = {{"A", "Продолжить", {90, 200, 110}}, {"SEL", "Выйти", {200, 200, 200}}};
    int x = 20;
    for (auto& h : hs) { g.button(x, Gfx::H - 14, h.b, h.c); g.text(h.t, x + 16, Gfx::H - 23, 15, INK); x += 36 + g.textWidth(h.t, 15) + 22; }
}
void Game::drawBuildMenu(Gfx& g) {
    auto list = buildable();
    int w = 340, h = 40 + (int)list.size() * 26, x = 20, y = 40;
    g.panel(x, y, w, h, PANEL, EDGE); g.text("Постройки", x + w / 2, y + 6, 18, EDGE, 1);
    for (int i = 0; i < (int)list.size(); i++) {
        auto& b = *list[i]; int ry = y + 34 + i * 26;
        if (i == menuSel) g.fill(x + 4, ry - 2, w - 8, 24, {110, 80, 40, 255});
        bool ok = canAfford(b.cost);
        g.text(b.name, x + 12, ry, 16, ok ? INK : Color{150, 110, 100});
        std::string cost; for (auto& kv : b.cost) cost += resName(kv.first) + " " + std::to_string(kv.second) + "  ";
        g.text(cost, x + w - 12, ry, 15, DIM, 2);
    }
}
void Game::drawSkills(Gfx& g) {
    g.fill(0, 0, Gfx::W, Gfx::H, {20, 14, 10, 245});
    g.text("Умения племени", Gfx::W / 2, 12, 24, EDGE, 1);
    g.text("Очки умений: " + std::to_string(skillPoints), Gfx::W / 2, 44, 18, INK, 1);
    for (int col = 0; col < 3; col++) {
        int x = 24 + col * 204;
        g.text(BRANCH_RU[col], x + 90, 76, 18, EDGE, 1);
        for (int row = 0; row < 3; row++) {
            int y = 106 + row * 100; auto* s = skillAt(col, row); if (!s) continue;
            bool sel = col == skillCol && row == skillRow, got = learned.count(s->id), can = canLearn(*s);
            Color fill = got ? Color{70, 110, 60, 255} : can ? Color{90, 70, 40, 255} : Color{45, 36, 30, 255};
            g.panel(x, y, 184, 76, fill, sel ? Color{255, 240, 120} : Color{120, 100, 70});
            g.text(s->name, x + 92, y + 6, 16, got || can ? INK : DIM, 1);
            g.text(s->desc, x + 92, y + 30, 12, DIM, 1);
            g.text(got ? "изучено" : "стоимость " + std::to_string(s->cost), x + 92, y + 52, 13, got ? Color{150, 230, 130} : EDGE, 1);
            if (row < 2) g.fill(x + 91, y + 76, 2, 24, {120, 100, 70});
        }
    }
    if (toastT > 0) { int w = g.textWidth(toast, 18) + 24; g.panel(Gfx::W / 2 - w / 2, 410, w, 30, PANEL, EDGE); g.text(toast, Gfx::W / 2, 414, 18, INK, 1); }
}
void Game::draw(Gfx& g) {
    g.clear({20, 20, 20});
    if (mode == Mode::Skills) { drawSkills(g); drawHints(g); return; }
    drawMap(g); drawHud(g);
    if (mode == Mode::Build) drawBuildMenu(g);
    if (mode == Mode::Pause) { g.fill(0, 0, Gfx::W, Gfx::H, {0, 0, 0, 150}); g.text("Пауза", Gfx::W / 2, 200, 36, INK, 1); }
    if (mode == Mode::Over) {
        g.fill(0, 0, Gfx::W, Gfx::H, {0, 0, 0, 170});
        g.text("Племя пало", Gfx::W / 2, 170, 40, {230, 90, 70}, 1);
        g.text("Вождь погиб на волне " + std::to_string(wavesSpawned), Gfx::W / 2, 232, 20, INK, 1);
        g.text("Нажмите SELECT, чтобы выйти", Gfx::W / 2, 270, 16, DIM, 1);
    }
    drawHints(g);
}
