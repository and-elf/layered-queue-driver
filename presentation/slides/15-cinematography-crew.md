---
layout: cards
chapter: Reel VIII · The Cinematography Crew
title: Filmteamet.
---

## Moduler
- core · config · platform
- hil · uds
- Samma manus, olika kameror

## Plattformar
- scripts/generators/platforms/zephyr.py
- stm32.py · esp32.py
- nrf52.py · samd.py · baremetal.py

Note:
Bakom varje regissör finns ett team. I filmen kallar vi DP — director of photography, fotograf — och så finns det specialister för varje aspekt: ljus, kamera, ljud, effekter.

I vår kodgenerator är teamet modulärt uppdelat. `core.py` är produktionsledaren — den orkestrerar. `config.py` läser DTS:en och bygger den interna modellen. `platform.py` väljer vilken kamera som ska användas. `hil.py` är vår testkamera. `uds.py` hanterar diagnostik.

Och i `platforms/`-mappen ligger varje "kamera": `zephyr.py`, `stm32.py`, `esp32.py`, `nrf52.py`, `samd.py`, `baremetal.py`. Samma manus — samma DTS — kan filmas med vilken som helst av dem. Du byter kamera med en kommandoradflagga.

Det är arkitekturen som gör det möjligt att portera till en ny plattform genom att skriva en ny modul, inte genom att modifiera generatorns kärna. Open-closed-principen i praktiken.
