---
layout: cards
chapter: Reel IX · Set Design
title: Scenografin — Input → Engine → Output.
---

## Takeaways
- Tunna lager mot hårdvara
- Pure processing-engine
- Pollning > ISR där det går

## Sökvägar
- src/drivers/lq_engine.c
- src/drivers/lq_hw_input.c
- include/lq_engine.h · lq_event.h

Note:
Bakom kulisserna måste alla scener spelas in i en scenografi. Och i vår kodbas är scenografin oerhört enkel — och det är det som gör den säker.

Tre lager. Inget mer.

Lager ett: ett tunt, RTOS-medvetet hårdvarulager. ISR:er och pollning. Det enda jobbet är att ta in data från fysiken och skuffa in den i en lock-free ringbuffer. Inga affärsregler här.

Lager två: en *ren* processing-engine. Pure. Inga RTOS-anrop, ingen dynamisk minnesallokering, ingen hårdvarukännedom. Bara logik som äter events från ringbuffern och spottar ut events till utgångskön. Det är detta lager som är unit-testbart, formellt verifierbart, kompilerbart till ren x86 för testning.

Lager tre: utgångsdrivers — också tunna, hårdvarunära.

En subtilitet: vi pollar där vi kan. GPIO, ADC, SPI, I2C — det är *deterministiskt* timad sampling, inte interrupt-driven. Endast asynkrona protokoll som CAN och UART använder ISR, eftersom meddelanden där kommer när de kommer. Determinism är inte en känsla, det är en arkitekturbeslut.
