local E = engine
local Gfx = require("src.gfx")
local C = Gfx.C

local M = {}
M.onExit = function() end

local state = {}
local W, H = 240, 320

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function hit(ax, ay, aw, ah, bx, by, bw, bh)
    return ax < bx + bw and ax + aw > bx and ay < by + bh and ay + ah > by
end

local function resetStar()
    state.sx = 34 + ((state.seed * 37) % 174)
    state.sy = 52 + ((state.seed * 73) % 206)
    state.seed = (state.seed * 17 + 11) % 997
end

function M.start()
    state.x = 112
    state.y = 154
    state.score = 0
    state.time = 40
    state.seed = math.floor(E.tick_ms()) % 997
    state.frame = 0
    state.anim = 0
    state.over = false
    state.win = false
    state.hazards = {
        { x = 42, y = 90, w = 24, h = 16, vx = 20 },
        { x = 156, y = 210, w = 28, h = 16, vx = -24 },
    }
    resetStar()
end

function M.update(dt)
    if state.over then return end
    state.time = state.time - dt
    if state.time <= 0 then
        state.time = 0
        state.over = true
    end

    state.anim = state.anim + dt
    if state.anim > 0.18 then
        state.anim = 0
        state.frame = (state.frame + 1) % 2
    end

    for i = 1, #state.hazards do
        local h = state.hazards[i]
        h.x = h.x + h.vx * dt
        if h.x < 28 then h.x = 28; h.vx = -h.vx end
        if h.x + h.w > 212 then h.x = 212 - h.w; h.vx = -h.vx end
        if hit(state.x, state.y, 14, 18, h.x, h.y, h.w, h.h) then
            state.time = math.max(0, state.time - 3)
            state.x, state.y = 112, 154
        end
    end

    if hit(state.x, state.y, 14, 18, state.sx, state.sy, 9, 9) then
        state.score = state.score + 1
        resetStar()
        if state.score >= 10 then
            state.win = true
            state.over = true
        end
    end
end

function M.draw()
    Gfx.paper()
    Gfx.hud(state.score, state.time)

    -- playfield card
    Gfx.panel(27, 43, 186, 224)

    -- doodle obstacles
    for i = 1, #state.hazards do
        local h = state.hazards[i]
        Gfx.rect(h.x, h.y, h.w, h.h, C.accent2)
        E.frame(math.floor(h.x), math.floor(h.y), h.w, h.h, C.ink)
        Gfx.rect(h.x + 4, h.y + 4, h.w - 8, 2, C.white)
    end

    Gfx.star(state.sx, state.sy, C.accent)
    Gfx.player(state.x, state.y, state.frame)

    if state.over then
        Gfx.panel(42, 112, 156, 98)
        Gfx.text_center(130, state.win and "YOU WIN!" or "TIME UP", state.win and C.green or C.accent2)
        Gfx.text_center(154, "SCORE " .. tostring(state.score), C.ink)
        Gfx.text_center(180, "5 RETRY / 0 MENU", C.ink2)
    else
        Gfx.text_center(280, "MOVE 2 4 6 8  |  0 PAUSE", C.ink2)
    end
end

function M.keypressed(k)
    if state.over then
        if k == "ok" then M.start()
        elseif k == "back" then M.onExit() end
        return
    end

    local step = 8
    if k == "left" then state.x = clamp(state.x - step, 30, 196)
    elseif k == "right" then state.x = clamp(state.x + step, 30, 196)
    elseif k == "up" then state.y = clamp(state.y - step, 46, 246)
    elseif k == "down" then state.y = clamp(state.y + step, 46, 246)
    elseif k == "back" then M.onExit() end
end

function M.keyreleased(_) end

return M
