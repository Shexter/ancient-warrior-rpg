-- Judgment: ends what is already failing. 3x ATK through armor below a quarter of the target's
-- HP, otherwise an ordinary ATK hit that armor still blunts.
return {
  cast = function(ctx)
    local t = ctx.target
    if t.hp * 4 < t.max_hp then
      ctx.damage(ctx.self.atk * 3, true)
    else
      ctx.damage(ctx.self.atk, false)
    end
  end,
}
