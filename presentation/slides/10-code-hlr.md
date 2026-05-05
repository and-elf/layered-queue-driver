---
layout: code
language: markdown
caption: requirements/high-level/safety-monitoring.md
---

# HLR-SAFE-001: Engine overheat protection
**ASIL:** D
**Rationale:** Förhindra katastrofal motorskada

Systemet ska detektera kylvätsketemperatur > 115 °C
inom 100 ms och initiera nödstopp.

Spårbarhet: → LLR-1.4-temp-monitor, LLR-3.2-shutdown

Note:
Här är ett verkligt HLR. Notera vad det inte säger.

Det säger ingenting om vilken ADC-kanal vi mäter på. Inget om vilket protokoll som ska kommunicera nödstoppet. Inget om hur vi implementerar timern. Inget om vilken plattform.

Det säger bara *vad* — beteendet — och *varför* — säkerhetsskälet. ASIL-nivån D säger oss att detta är högsta säkerhetsklassen. Spårbarhetslänkarna pekar på de LLR:er som tar hand om detaljerna.

En domänexpert kan skriva detta. En jurist kan läsa det. En revisor från TÜV kan auditera det. Ingen av dem behöver kunna C.

Det är hela poängen.
