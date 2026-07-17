// Assembles the flasher page into web/dist/.
//
// esp-web-tools ships a pre-bundled, self-contained ESM build in dist/web:
// relative imports only, with the per-chip flasher stubs code-split alongside.
// Copying it wholesale is what keeps the deployed page free of any CDN.
//
// Deliberately not re-bundling it from source: esp-web-tools depends on
// @material/web ^2.2.0, and 2.5.0 renamed the *-styles.js files it imports to
// *-styles.cssresult.js, so a fresh bundle fails to resolve them. The upstream
// pre-bundled output has that already baked in.

import { cp, mkdir, rm, readdir } from "node:fs/promises";

const OUT = "dist";
const ESP_WEB_TOOLS = "node_modules/esp-web-tools/dist/web";
const PAGE_FILES = ["index.html", "app.js", "style.css"];

await rm(OUT, { recursive: true, force: true });
await mkdir(OUT, { recursive: true });

await cp(ESP_WEB_TOOLS, OUT, { recursive: true });

for (const file of PAGE_FILES) {
  await cp(file, `${OUT}/${file}`);
}

const written = await readdir(OUT);
console.log(`flasher built into web/${OUT}/ (${written.length} files)`);
