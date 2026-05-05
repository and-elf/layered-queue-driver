---
layout: cards
chapter: Reel X · Dr. Strangelove
title: Stop worrying. Love the generator.
---

## Takeaways
- Ingen AI vid generering
- Bit-identisk output
- Tool qualification: ISO 26262-8

## Bevis
- 444 tester
- 92–100% kritisk täckning
- gitleaks · ruff (hooks)

Note:
Stanley Kubricks "Dr. Strangelove or: How I Learned to Stop Worrying and Love the Bomb" handlar om att lära sig leva med teknologi som är skrämmande men oundviklig. Idag måste vi lära oss något liknande om kodgenerering.

Den första frågan jag får är alltid: "Använder ni AI för att skriva säkerhetskritisk kod?" Svaret är *nej*. AI hjälpte till att bygga *generatorn*. Generatorn själv är ren, deterministisk Python. Samma `requirements/` och samma version av generatorn ger bit-identisk `lq_generated.c`. Du kan checka in genererad kod i Git och se den som data, inte som artefakt.

Det här passar perfekt in i ISO 26262-8: Tool Qualification. Du behöver inte certifiera *koden* den genererar — du certifierar *generatorn*, en gång, och sedan kan vilken kod den genererar som helst räknas som korrekt så länge inputen är korrekt.

Vi har 444 automatiserade tester på generatorn själv. 92 till 100% täckning av kritiska paths. Pre-commit-hooks med gitleaks och ruff. Generatorn behandlas som vad den är: säkerhetskritisk mjukvara.
