---
layout: cards
chapter: Reel XIII · The Distributor
title: CI är distributören.
---

## Takeaways
- Samma kommando som dev
- Ingen "works on my machine"
- Coverage-badge alltid synkad

## Sökvägar
- .github/workflows/
- docs/CI_HIL_SETUP.md
- pre-commit: gitleaks + ruff

Note:
När filmen är klipplagd och godkänd är distributören som tar den vidare till biograferna. För oss är distributören CI-pipelinen.

Det viktiga med vår CI är att den inte gör något *speciellt*. Den kör exakt samma kommandon som du och jag kör lokalt: `west init`, `west update`, `west build`, `ctest`. Inga särskilda CI-mode-flaggor som drifter över tid. Det betyder ingen "works on my machine" — om bygget passerar lokalt, passerar det i CI. Om CI misslyckas, kan du reproducera felet på din laptop på minuter.

Coverage-badgen i README:n regenereras automatiskt vid varje merge. Den är aldrig osynkad med koden. Du har inga gamla siffror som ljuger.

På ingångssidan har vi pre-commit-hooks. `gitleaks` förhindrar att hemligheter checkas in. `ruff` linter och formaterar Python-koden i generatorn. Hookar är inte valbara — det är en organisationsregel hos oss, eftersom säkerheten på *verktyget* är säkerheten på *produkten*.
