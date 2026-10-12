-- Medusa's turn (also used by Avaritia until it gets its own behavior). It must reproduce the
-- core golden battles exactly, including the order of random draws: one roll, then the attack,
-- then either regeneration or a second roll for an extra turn.
return {
  take_turn = function(ctx)
    local roll = ctx.rng(10)
    ctx.attack()
    if ctx.over() then return end
    if roll < 5 then
      ctx.heal("self", ctx.self.life) -- Strike: regenerate
    elseif ctx.rng(10) < 6 then
      ctx.extra_turn() -- Gaze: strike again at once
    end
  end,
}
