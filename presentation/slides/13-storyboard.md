---
layout: cards
chapter: Reel VII · The Storyboard
title: Device Tree är vår storyboard.
---

## Takeaways
- Komplett, deterministisk beskrivning
- Genereras från krav
- Läses även av Zephyr-kärnan

## Sökvägar
- ${BUILD}/app.dts
- samples/automotive/app.dts
- dts/bindings/

Note:
Mellan manuset och inspelningen ligger storyboarden. För filmare är det en serie tecknade rutor, en per scen, som visar exakt vad kameran ska se. Det är vad regissören delar med ljusmästaren och scenografen och kostymdesignern.

I vårt system är storyboarden Device Tree. DTS är ett gammalt format från Linux-världen som beskriver hårdvara hierarkiskt. Zephyr använder det för allt — vilka pinnar är vad, vilka bussar finns, hur är de kopplade.

Vi har utökat det. Vår DTS beskriver inte bara hårdvaran utan hela databehandlingsdiagrammet: ingångar, mid-level-validatorer, voters, range-checkers, utgångar, deadlines.

Det fina är att DTS:en *redan läses* av Zephyrs egen kärna och drivers. Vi piggybackar på ett standardformat som varje embedded-utvecklare redan kan. Inget nytt språk att lära sig. Och det är genererat — inget vi handskriver.
