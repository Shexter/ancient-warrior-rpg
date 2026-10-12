-- 1.5x ATK, through armor. `//` is integer division: engine operations take whole numbers.
return { cast = function(ctx) ctx.damage(ctx.self.atk * 3 // 2, true) end }
