# mark
arena shooter (arcade dir igre)
VS (Windows), biblioteka (SDL3), jezik (C)

**Pokretanje**
  Iz VS-a, ali je potrebno rucno podesit SDL3
    1. Desni klik na projekt -> properties
    2. **C/C++** -> General -> Addiotional Include Directories (path/do/SDL3/include)
    3. **Linker** -> General -> Addiotional Library Directories (path/do/SDL3/lib/x64)
    4. **Linker** -> Input -> Additional Dependencies (dodat SDL3.lib)

## FUNKCIONALNOSTI:
  main menu
  death screen (game over)
  osnovni UI/HUD (HP, ammo, reload (trenutno 18/inf), kill count (scoreboard metrika)
  vise tipova neprijatelja (obicni, mali brzi, tenk, i lik sta puca (archer iako nema strijele))
  mehanike: pucanje, dashanje i regen (potrebno balance outat dodatno)
  vizualni feedback (damage nums, dash indikator)
  spremanje rezultata (arcade-style lb)


## KONTROLE:
  **WASD** - movement
  **LMB/CTRL** - shoot
  **SPACE** - dash
  **R** - reload
  **ENTER** - start/return

possible update:
  duze trajanje (wave-ovi umjesto constant spawna)
  boos nakon odredenog broja killova
  zvuk (efekti i glazba)
  sustav tezine (myb ne zato sta je arcade style igra)
  postavke igre (volume, kontrole)
  druga metrika bodovanja (combo, bonus bodovi) 
  ...
