---
layout: code
language: cmake
caption: cmake/RequirementsDriven.cmake — "Action!"
---

add_lq_application_from_requirements(my_app
  REQUIREMENTS requirements/
  PLATFORM    zephyr
  RTOS        zephyr
  STRICT
)

Note:
Det här är hela anropet. Sex rader.

`REQUIREMENTS` pekar på en mapp där HLR och LLR ligger. `PLATFORM` och `RTOS` styr vilka generatorer som körs — vi kan byta från `zephyr` till `stm32` till `esp32` med en enradsändring. `STRICT` säger att varningar är fel.

Det som händer under huven är att CMake först kör `reqgen.py validate`, sedan `reqgen.py generate-dts`, sedan kallas vidare till `add_lq_application()` som hanterar resten — DTS-parsning, C-kodgenerering, kompilering, länkning.

Det vackra är: om vi behöver byta plattform, ändra ett tröskelvärde, eller lägga till en ny redundansvotare, så är detta den enda raden vi rör vid på CMake-nivån. Resten flödar uppåt från kraven.
