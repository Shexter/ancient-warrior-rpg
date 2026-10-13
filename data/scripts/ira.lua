-- Ira: hits harder the more it bleeds (ATK x (100 + 100 x missing HP / max HP) / 100). Below half
-- HP, on a 1-in-2 roll it charges, then strikes for twice that. Every hit varies by +/-15%.
local function vary(ctx, power) return power * (85 + ctx.rng(31)) // 100 end
return {
  take_turn = function(ctx)
    local s = ctx.self
    local power = s.atk * (100 + 100 * (s.max_hp - s.hp) // s.max_hp) // 100
    if s.has("charging") then
      ctx.attack(vary(ctx, power * 2))
    elseif s.hp * 2 < s.max_hp and ctx.rng(2) == 0 then
      ctx.say("Ira's rage boils over...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.attack(vary(ctx, power))
    end
  end,
}
