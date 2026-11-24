-- Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
-- nucleo64/programas/musl/prueba.lua - Hispa-64, etapa 7: lo que hace Lua
-- (terceros/lua, compilado sin cambiar nada con musl) en Hispa-64.
local bien, mal = 0, 0
local function mira(c, d)
  if c then bien = bien + 1 else mal = mal + 1; print("  MAL: " .. d) end
end
print("LUA: " .. _VERSION .. " en Hispa-64 (con musl, sin cambiar nada)")
mira(#arg == 2 and arg[1] == "uno" and arg[2] == "dos", "los argumentos")
mira(string.format("%5.2f|%d|%s|%x", math.pi, 42, "ñandú", 255) == " 3.14|42|ñandú|ff", "string.format")
mira(utf8.len("canción") == 7, "utf8.len")
local t = {}
for i = 1, 10000 do t[i] = (i * 7919) % 10007 end
table.sort(t)
local ordenada = true
for i = 2, #t do if t[i - 1] > t[i] then ordenada = false end end
mira(ordenada, "table.sort de 10.000")
local function contador()
  local n = 0
  return function() n = n + 1; return n end
end
local c = contador(); c(); c()
mira(c() == 3, "las clausuras")
local co = coroutine.wrap(function() for i = 1, 3 do coroutine.yield(i * i) end end)
mira(co() + co() + co() == 14, "las corrutinas")
local ok, err = pcall(function() error("a propósito") end)
mira(not ok and err:find("a propósito"), "pcall y error")
mira(("hola mundo"):gsub("o", "0") == "h0la mund0", "string.gsub")
mira(2^10 == 1024 and 7 // 2 == 3 and (0xF0 | 0x0F) == 0xFF and math.type(1) == "integer", "las cuentas")
mira(math.abs(math.sqrt(2) - 1.41421356) < 1e-6, "math.sqrt")
mira(os.time() > 1767225600, "os.time")
mira(os.clock() >= 0, "os.clock")
local f = io.open("/SALUDO.TXT")
mira(f and f:read("l"):sub(1, 4) == "Hola", "io.open y read")
if f then f:close() end
mira(io.open("/NO-ESTA.TXT") == nil, "io.open de uno que no está")
mira(load("return 1 + 1")() == 2, "load")
local s = {}
for i = 1, 100000 do s[i] = tostring(i) .. "x" end
s = nil
collectgarbage()
mira(collectgarbage("count") > 0, "el recolector de basura")
if mal == 0 then
  print(string.format("LUA: todo bien (%d pruebas); hoy es %s", bien, os.date("!%d/%m/%Y", os.time())))
else
  print("LUA: " .. mal .. " mal de " .. (bien + mal))
end
os.exit(mal == 0 and 0 or 1)
