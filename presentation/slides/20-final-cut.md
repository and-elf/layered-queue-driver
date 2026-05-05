---
layout: cards
chapter: Reel XII · The Final Cut
title: Action — kameran rullar.
---

## Takeaways
- Klipp manuset
- west build (Action)
- west flash + ctest (Premiär)

## Sökvägar
- scripts/build_firmware.sh
- scripts/west_build_with_prjconf.sh
- build/ · Testing/

Note:
Nu sätter vi ihop allt. Ett verkligt arbetsflöde.

Steg ett: någon — en säkerhetsanalytiker, en domänexpert — öppnar ett LLR och justerar ett toleransvärde. En liten textändring. Commit, push, pull request.

Steg två: jag, eller CI, kör `west build`. Och nu händer magin. CMake konfigurerar. Vår RequirementsDriven.cmake aktiveras. `reqgen.py` validerar i STRICT-läge. Genererar DTS. `dts_gen.py` genererar C-koden. Ninja kompilerar. Linker länkar. Ut kommer en `zephyr.elf`.

Steg tre: `west flash` skickar binären till hårdvaran. `ctest` kör hela testsviten. 444 av 444 grönt. Och vi har en uppdaterad produkt.

Mellan steg ett och steg tre har ingen människa skrivit en rad C-kod. Ingen har uppdaterat en spårbarhetsmatris. Ingen har skrivit ett test. Ingen har uppdaterat dokumentationen. Allt detta är *biprodukter* av byggprocessen.
