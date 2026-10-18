-- Sanctuary: Guard for 2 turns and heal 20% of max HP.
return {
  cast = function(ctx)
    ctx.apply_status("self", "guard", 2)
    ctx.heal("self", ctx.self.max_hp * 20 // 100)
  end,
}
