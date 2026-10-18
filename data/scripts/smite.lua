-- Smite: ATK through armor, tripled against an enemy that is gathering its strength.
return {
  cast = function(ctx)
    local power = ctx.self.atk
    if ctx.target.has("charging") then power = power * 3 end
    ctx.damage(power, true)
  end,
}
