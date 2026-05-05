---
layout: cards
chapter: Reel I · A New Hope
title: A long time ago, in a codebase far, far away…
---

## Takeaways
- Krav → spec → arkitektur → kod → test
- Sex roller, sex tolkningar
- Spårbarhetsmatrisen ingen orkar

## I detta repo
- requirements/high-level/
- requirements/low-level/
- en källa, många artefakter

Note:
Tänk er Star Wars opening crawl: "A long time ago, in a galaxy far, far away…" och sedan rullar texten uppåt i lutande perspektiv. Bakgrunden för det vi gör.

Den traditionella vägen från krav till kod ser ungefär ut så här: en produktägare skriver "systemet ska detektera överhettning inom 100 ms". En systemingenjör tolkar det till en specifikation. En arkitekt designar en lösning. En utvecklare implementerar i C. En testare validerar — om vi har tur. En teknisk skribent försöker hänga med.

Sex roller, sex tolkningar, sex artefakter — och en spårbarhetsmatris som ingen mänsklig själ orkar hålla uppdaterad.

I detta repository finns bara två platser där krav lever: high-level som markdown, low-level som YAML. Allt annat — DTS, C-kod, tester, dokumentation — är genererat. En källa, många artefakter. Det är hjältarnas resa vi ska följa idag.
