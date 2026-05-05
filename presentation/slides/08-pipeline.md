---
layout: cards
chapter: Reel IV · The Pipeline
title: Manus → Premiär.
---

## Steg
- Krav (YAML/MD)
- DTS (storyboard)
- C + tester (inspelat)
- ELF (final cut)

## Verktyg
- scripts/reqgen.py
- scripts/dts_gen.py
- scripts/generators/
- west build → ninja

Note:
Innan vi går in på detaljer — låt mig rita upp hela flödet på en bild.

Steg ett: kraven, som markdown och YAML i `requirements/`. Det är manuset.

Steg två: `reqgen.py` validerar och översätter dem till DTS — Device Tree Source. Det är vår storyboard, en deterministisk beskrivning av vad varje bild i systemet ska innehålla.

Steg tre: `dts_gen.py` läser DTS:en och genererar C-kod. För Zephyr körs den genom modulen `scripts/generators/platforms/zephyr.py`. För STM32 körs `stm32.py`. Samma manus, olika kameror.

Steg fyra: `west build` orkestrerar CMake och Ninja, kompilerar, länkar, och spottar ut en `zephyr.elf`. Det är vår final cut.

Tester genereras parallellt, från samma krav. När vi flashar binären på riktig hårdvara kör vi också HIL-tester som genererats från samma källa. Symmetriskt. Det är den V-modell som vi alltid pratat om men sällan uppnått.
