---
layout: code
language: bash
caption: en tagning — krav → flashbar binär
---

$ vim requirements/low-level/llr-1.1-rpm-sampling.yaml

$ west build -b nucleo_f429zi samples/automotive
   → reqgen.py validate (STRICT)
   → reqgen.py generate-dts → build/prereq/app.dts
   → dts_gen.py            → lq_generated.[ch]
   → ninja                 → zephyr.elf

$ west flash
$ ctest --test-dir build         # 444 / 444

Note:
Här är hela kommandorad-sekvensen.

Det första kommandot — `vim` — är den enda mänskliga insatsen. Resten är automatik. Lägg märke till hur `west build` triggar en kedja av Python-skript: validate, generate-dts, dts_gen. Och därefter Ninja som kompilerar.

Det är värt att stanna upp på `STRICT` här. Det är rekommenderat läge för CI och för release-byggen. Det betyder att en varning från konflikt-handlern stoppar bygget. En mjukare `NORMAL` kan vara rimlig under utveckling när du vet att du har konflikter du jobbar på.

Och `444 / 444` på sista raden — det är inte en marknadsföringssiffra. Det är det verkliga antalet tester som körs på varje commit. Misslyckas en — bygget rött. Misslyckas inga — färdigt att flasha.

Det här är vad "smidig och säker" betyder i praktiken: en commit, en kommando, en deterministisk binär.
