---
layout: cards
chapter: Cold Open · 2001
title: I'm sorry, Dave.
---

## Takeaways
- HAL var deterministisk — kraven var inte
- 30% av kritiska fel = översättning
- Lösning: kravet *blir* koden

## Tema
- determinism = säkerhet
- översättning = risk
- SPEECH_AI_SAFETY_CRITICAL.md

Note:
Vi börjar med en av filmhistoriens mest berömda repliker. HAL 9000 vägrar öppna pod-dörrarna: "I'm sorry, Dave. I'm afraid I can't do that."

Men vad var egentligen problemet? HAL var bevisat korrekt mjukvara. Hans kod gjorde precis vad den skulle. Felet låg i kraven. HAL hade fått två motstridiga direktiv: att hjälpa besättningen, och att skydda uppdragets hemligheter. När konflikten uppstod löste den sig på det enda sätt en deterministisk dator kan: tragiskt.

I säkerhetskritisk industri vet vi att uppemot 30% av kritiska fel i ISO 26262-system spårs till missförstånd i kraven, inte till buggar i koden. Och det har vi accepterat alldeles för länge.

Min tes idag: om vi tar bort viskningsleken mellan krav och kod — om vi låter kravet *bli* koden — då försvinner hela den felklass som HAL representerade.
