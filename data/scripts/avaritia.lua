-- Avaritia: attacks; on a 1-in-4 roll it counts its coins and charges, then next turn strikes
-- for 2.5x ATK. Charging is applied for one turn, so it expires at the end of the strike turn.
-- Every hit varies by +/-15%.
local function vary(ctx, power) return power * (85 + ctx.rng(31)) // 100 end
return {
  take_turn = function(ctx)
    if ctx.self.has("charging") then
      ctx.attack(vary(ctx, ctx.self.atk * 5 // 2))
    elseif ctx.rng(4) == 0 then
      ctx.say("Avaritia counts its coins...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.attack(vary(ctx, ctx.self.atk))
    end
  end,
}
