---
layout: cards
chapter: Reel V · The Script
title: Manuset — High-Level Requirements.
---

## Takeaways
- Markdown av domänexperter
- ID · ASIL · rationale
- Versionerat med koden

## Sökvägar
- requirements/high-level/*.md
- HLR-* IDn länkar till LLR

Note:
Nu öppnar vi manuset.

High-Level Requirements skrivs i markdown av de som faktiskt vet *vad* systemet ska göra: säkerhetschefer, systemingenjörer, produktägare. Inte programmerare. Det är en viktig poäng — programmerare ska inte tolka kraven, de ska implementera dem.

Varje HLR har ett ID, en ASIL-nivå (eller motsvarande för andra standarder), en rationale, och en länkad lista av LLR:er som realiserar den. Allt versionerat i Git, granskat i samma pull request som koden.

Det betyder att kraven inte är ett dokument som finns någonstans i Confluence och som någon glömmer uppdatera. De är källkod. När säkerhetsanalytikern ändrar något i kraven, går det genom samma review-process, samma CI-pipeline, samma godkännandekedja som varje annan kodändring.

Och om kravet ändras — så regenereras allt. Kod, tester, dokumentation. Synkat.
