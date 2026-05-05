---
layout: code
language: text
caption: arkitektur — två tunna HW-lager, ren kärna
---

ISR / Polling
   │ lq_hw_push()
Lock-free ringbuffer
   │ lq_hw_pop()
Mid-level vtables   (PURE)
   │
ENGINE STEP        (deterministic)
   │
Output dispatch    (bounded)
   │
CAN · J1939 · CANopen · GPIO · PWM · DAC · SPI · I2C · UART · Modbus

Note:
Här är samma sak som ett ASCII-diagram.

Notera ordet PURE. Mellan-lagret — där voters, validators och range-checkers bor — är ren kod. Inga locks, inga mutexes, inga RTOS-anrop. Det betyder att det går att köra exakt samma kod på en utvecklarmaskin som i firmwaren. Vi kompilerar samma kärna med GCC på Linux för unit-tester, och med arm-none-eabi-gcc för målsystemet. Identisk bytkod-logik.

ENGINE STEP är det centrala. Ett anrop per cykel. Ingen multitasking inuti, ingen icke-determinism. Tar in events, spottar ut events. Bounded execution time — vi kan räkna ut den värsta tänkbara körtiden statiskt.

Output dispatch är en simpel for-loop. Inget jobbflöde, inga köer. När engine-step är klar, dispatchar vi alla utgångsevent som är schemalagda för den cykeln. Det är allt.

Det är inte raketforskning. Det är gammal aerospace-arkitektur, DO-178C-stil. Men *det är genererat från krav*.
