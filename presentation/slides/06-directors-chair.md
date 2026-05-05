---
layout: cards
chapter: Reel III · The Director's Chair
title: Regissörsstolen.
---

## Takeaways
- En CMake-funktion, en entry point
- NORMAL · STRICT · AUTO_FIX · FORCE
- Hookar in före build

## Sökvägar
- cmake/RequirementsDriven.cmake
- cmake/LayeredQueueApp.cmake
- add_lq_application_from_requirements()

Note:
Studion är förberedd. Nu sätter sig regissören i stolen.

I vårt fall är regissörsstolen en CMake-funktion: `add_lq_application_from_requirements()`. Du anropar den en gång i din `CMakeLists.txt`. Du pekar på en mapp med krav, anger plattform och RTOS, och du är klar. Allt annat är delegerat.

Den har fyra lägen:
*NORMAL* bygger på dina krav som de är. *STRICT* behandlar varningar som fel — bra för CI. *AUTO_FIX* försöker lösa konflikter automatiskt. *FORCE* bygger även med fel — det vill man aldrig i produktion, bara för felsökning.

Den hookar in i Zephyrs `${TARGET}_codegen` så att ingen kompilering ens *startar* innan kraven är validerade. Konflikter, motstridiga toleranser, saknade länkar mellan HLR och LLR — allt fångas här. Som en regissör som vägrar säga "Action" om manuset inte är klart.
