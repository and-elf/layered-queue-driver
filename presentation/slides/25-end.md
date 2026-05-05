---
layout: end
title: The End.
subtitle: Frågor är välkomna · git clone är ännu varmare
left_label: Entry points
right_label: Manus & scen
---

## Entry points
- cmake/RequirementsDriven.cmake
- scripts/reqgen.py
- west.yml

## Manus & scen
- requirements/high-level/
- requirements/low-level/
- samples/automotive/

Note:
Tack för uppmärksamheten.

Fyra platser i repot är värda en personlig titt om ni vill gå vidare:

För det första, `cmake/RequirementsDriven.cmake` — själva regissörsstolen, det enda CMake-anropet ni behöver veta om.

För det andra, `scripts/reqgen.py` — kärnan i kravbehandlingen och konfliktdetekteringen.

För det tredje, `west.yml` — manifestet som binder ihop hela studion.

Och för det fjärde, `samples/automotive/` — ett komplett, körbart exempel: en motorövervakare med dubbla RPM-givare, median-voting och J1939-utgång. Bygg och kör det själva — det är det bästa sättet att förstå vad jag har pratat om.

Frågor?

Och som sagt: en `git clone` är välkomnare än en fråga, för då kan ni ge feedback också.
