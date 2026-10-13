-- Gula: every hit heals it by half the damage dealt; on a 1-in-3 roll it opens its maw and
-- charges, then devours for 2x ATK and heals all the damage dealt. Every hit varies by +/-15%.
local function vary(ctx, power) return power * (85 + ctx.rng(31)) // 100 end
return {
  take_turn = function(ctx)
    if ctx.self.has("charging") then
      ctx.heal("self", ctx.attack(vary(ctx, ctx.self.atk * 2)))
    elseif ctx.rng(3) == 0 then
      ctx.say("Gula opens its maw...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.heal("self", ctx.attack(vary(ctx, ctx.self.atk)) // 2)
    end
  end,
}
