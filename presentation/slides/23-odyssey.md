---
layout: cards
chapter: Reel XIV · A Determinism Odyssey
title: Vad det betyder.
---

## Takeaways
- Designa, inte översätta
- "What if" blir billigt
- Spårbarhet som biprodukt

## Konsekvens
- Ny plattform → samma manus
- 10–15 h byggtid med AI-stöd
- Ekonomisk barriär: borta

Note:
Slutet närmar sig. Vad har vi sett?

Det första: ingenjörer slutar översätta och börjar designa. Tiden som tidigare gick åt till att skriva ISR-handlers och voter-logik fri från fel går nu åt till att verkligen *fundera* över systembeteende. Det är ett yrkesskifte — från hantverkare till arkitekt.

Det andra: "What if" blir billigt. Vad händer om vi byter från ASIL B till ASIL D? Regenerera och kör testerna. Vad händer om vi byter från CAN till J1939? Ändra protokoll-fältet i LLR:en. Vad händer om vi vill stödja en fjärde plattform? Skriv en ny generator-modul. Iteration som kostade veckor kostar nu minuter.

Det tredje, och kanske viktigaste: spårbarhet är en biprodukt. Inte ett dokumentationsprojekt som körs i sista minuten innan TÜV-inspektion. Det är inneboende i hur vi bygger.

Och slutligen: hela det här verktyget byggdes på 10 till 15 timmar med AI-assistans. Den ekonomiska barriären — som tidigare var "200 timmar och vi har inte budget" — är borta. Det är inte AI som skriver din säkerhetskritiska kod. Det är AI som gjorde det möjligt att bygga *verktyget* som genererar den.
