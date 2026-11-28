// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// hispa64/cripto-preludio.js - Hispa-64, etapa 10 i (docs/64/HISPA-64.md, sección 26):
// lo que va delante del include.js de las pruebas de Ladybird en «MWEB --cripto»
// (mwebcripto.c): un «internals» nuestro (el de test-web, aquí, no hay). Al
// acabar una prueba, manda su texto a MWEB y pasa a la siguiente.
globalThis.internals = {
    signalTestIsDone(texto) {
        const nombre = location.pathname.split("/").pop().replace(/\.html$/, "");
        const sigue = () => location.replace("/siguiente/" + nombre);
        fetch("/resultado/" + nombre, { method: "POST", body: texto }).then(sigue, sigue);
    },
    spoofCurrentURL(url) {},
    gc() {},
    gcAsync() {
        return Promise.resolve();
    },
};
