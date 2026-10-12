-- Every third turn the Cyclops throws a boulder for double damage; otherwise it attacks.
return {
  take_turn = function(ctx)
    if ctx.turn % 3 == 0 then
      ctx.say("The Cyclops hurls a boulder!")
      ctx.damage(ctx.self.atk * 2, true)
    else
      ctx.attack()
    end
  end,
}
