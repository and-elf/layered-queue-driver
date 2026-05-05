---
layout: cards
chapter: Reel VI · The Shot List
title: Shot list — Low-Level Requirements.
---

## Takeaways
- Strukturerad YAML
- Konflikter detekteras automatiskt
- Ändra → regenerera → klart

## Sökvägar
- requirements/low-level/*.yaml
- scripts/reqgen.py
- scripts/conflict_handler.py

Note:
Om HLR är manuset är LLR shot list — den detaljerade listan över exakt vilka tagningar som ska göras.

LLR är YAML. Strukturerad, maskinläsbar, men fortfarande mänsklig — ingen kod, ingen pseudokod. Bara fakta: den här ADC-kanalen, det här tröskelvärdet, den här toleransen, det här protokollet, den här hertzfrekvensen.

Varje LLR har en `parent` som pekar tillbaka till sitt HLR. Det betyder att spårbarheten är *inneboende* — du kan inte skriva ett LLR utan att deklarera vilket HLR det realiserar.

Det andra som händer är konfliktdetektering. Modulen `conflict_handler.py` läser alla LLR:er och letar efter motstridigheter — samma signal med olika gränser, två LLR:er som båda hävdar äganderätt över samma utgång, dödläsningar i schemaläggningen. När en konflikt hittas i STRICT-läge stannar bygget. Du kan inte producera en binär från motstridiga krav.
