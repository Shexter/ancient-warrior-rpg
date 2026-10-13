-- Avaritia: attacks; every 4th turn counts its coins and charges, then strikes for 2.5x ATK.
-- Charging is applied for one turn, so it expires at the end of the strike turn.
return {
  take_turn = function(ctx)
    if ctx.self.has("charging") then
      ctx.damage(ctx.self.atk * 5 // 2, true)
    elseif ctx.turn % 4 == 0 then
      ctx.say("Avaritia counts its coins...")
      ctx.apply_status("self", "charging", 1)
    else
      ctx.attack()
    end
  end,
}
