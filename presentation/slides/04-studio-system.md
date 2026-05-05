---
layout: cards
chapter: Reel II · The Studio System
title: West är studion.
---

## Takeaways
- Manifest binder Zephyr + moduler
- west build = "Action"
- Modulen exponeras till Zephyr

## Sökvägar
- west.yml
- zephyr/module.yml
- zephyr/CMakeLists.txt

Note:
För att förstå inspelningen måste vi först förstå studion. I vår värld heter studion *west*.

West är Zephyr-projektets verktyg för att hämta källkod, bygga, flasha och felsöka. Det är som ett gammalt Hollywood-studiosystem: alla avdelningar — kameran, ljuset, scenografin, ljudet — koordineras under ett tak. När du skriver `west build` är det "Action!" — alla avdelningar börjar samtidigt rulla.

Filen `west.yml` är studions kontraktsbok. Den binder ihop vårt repository med Zephyr-kärnan och alla beroenden, alla på exakta versioner. Inget får drifta.

Och det här repot exponeras till Zephyr som en modul via `zephyr/module.yml` och `zephyr/CMakeLists.txt`. Det innebär att när någon annan kör `west build` på sitt projekt, blir layered-queue-driver automatiskt en del av deras inspelning. Som ett produktionsbolag som hyrs in.
