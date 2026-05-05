---
layout: cards
chapter: Reel XI · The Test Screening
title: V-modellen sluts.
---

## Vänster sida — SW
- Unit-tester per LLR
- GoogleTest · CTest
- Mock av HW-gränssnitt

## Höger sida — HW
- tests/hil/
- scripts/hil_test_gen.py
- generate_comprehensive_hil_tests.py

Note:
Före premiären har varje stor film en testvisning. Publik bjuds in, reagerar, och regissören klipper om baserat på reaktioner.

För oss är det dubbla testvisningar — V-modellens båda sidor.

Vänster sida, mjukvaruverifiering: unit-tester, ett per LLR, autogenererade. GoogleTest under CTest. Mockade hårdvarugränssnitt så vi kan köra på vilken laptop som helst. När du ändrar ett LLR, regenereras dess test, och CI fångar regression.

Höger sida, hårdvaruverifiering: HIL-scenarier, också autogenererade från samma LLR. Skickas till en testrigg med riktig STM32, riktig CAN-buss, riktig sensor. Vi injicerar 120 °C på temperatursensorn — verifierar att shutdown sker inom 100 ms.

Det här är V-modellens heliga gral: *bevisad korrespondens mellan krav och verifiering*. Inte hoppat på, utan inneboende. Samma rad i samma fil genererar både implementation och dess test. Du kan inte ändra ena utan den andra. Och du kan inte få ut en grön CI utan båda.
