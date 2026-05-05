---
layout: code
language: dts
caption: app.dts — autogenererad från krav
---

rpm_adc: lq-hw-adc-input@0 {
    compatible = "lq,hw-adc-input";
    io-channels = <&adc0 0>;
    stale-us   = <5000>;
};

rpm_merge: lq-mid-merge@0 {
    compatible    = "lq,mid-merge";
    sources       = <&rpm_adc &rpm_spi>;
    voting-method = "median";
    tolerance     = <50>;
};

Note:
Här är delen av DTS:en som motsvarar vårt LLR från förra sliden.

Två noder: en ADC-ingång och en merge-nod som tar två sourcer och voterar. Allt med `compatible`-strängar som matchar våra Zephyr-bindings i `dts/bindings/`.

Den intressanta detaljen: Zephyrs *egen* DTS-kompilator validerar den här filen mot bindingsen. Det betyder att om generatorn genererar något som inte är välformat — fel typ på ett fält, saknad property — så fångas det redan av Zephyr, innan vår C-kodgenerator ens körs.

Vi får alltså validering i två lager: vår egen `reqgen.py validate` på krav-nivå, och Zephyrs DTS-validering på arkitektur-nivå. Två oberoende grindar. Båda måste passera innan en enda C-rad genereras.
