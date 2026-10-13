-- Ira: hits harder the more it bleeds (ATK x (100 + 100 x missing HP / max HP) / 100). Below half
-- HP it charges every other turn, then strikes for twice that.
return {
  take_turn = function(ctx)
    local s = ctx.self
    local power = s.atk * (100 + 100 * (s.max_hp - s.hp) // s.max_hp) // 100
    if s.has("charging") then
      ctx.damage(power * 2, true)
    elseif s.hp * 2 < s.max_hp and ctx.turn % 2 == 0 then
      ctx.say("Ira's rage boils over...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.damage(power, true)
    end
  end,
}
