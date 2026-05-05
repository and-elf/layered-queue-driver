---
layout: code
language: yaml
caption: west.yml — studions kontraktsbok
---

manifest:
  version: "0.5"
  self:
    path: .
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: main
      import: true

Note:
Här är hela manifestet. Det är allt som behövs.

Notera att `import: true` betyder att vi ärver hela Zephyrs egen manifest — alla deras moduler, alla deras toolchain-definitioner. Vi behöver inte lista varje hjälpbibliotek, det sköter Zephyr.

Två kommandon räcker för att klona hela världen:

```
west init -l .
west update
```

Och därefter är allt vi behöver tillgängligt på exakta, reproducerbara versioner. Detta är grunden för allt annat vi ska prata om — utan reproducerbart byggsystem är determinism omöjlig.
