-- Gula: every attack heals it by half the damage dealt; every 3rd turn it opens its maw and
-- charges, then devours for 2x ATK and heals all the damage dealt.
return {
  take_turn = function(ctx)
    if ctx.self.has("charging") then
      ctx.heal("self", ctx.damage(ctx.self.atk * 2, true))
    elseif ctx.turn % 3 == 0 then
      ctx.say("Gula opens its maw...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.heal("self", ctx.attack() // 2)
    end
  end,
}
