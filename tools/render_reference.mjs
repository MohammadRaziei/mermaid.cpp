import chromium from '@sparticuz/chromium';
import puppeteer from 'puppeteer-core';
import fs from 'fs';
const [,, mmd, out, layout='dagre', id='basic'] = process.argv;
const browser = await puppeteer.launch({args: chromium.args, executablePath: await chromium.executablePath(), headless: 'shell'});
const page = await browser.newPage();
await page.setContent('<html><body><div id="c"></div></body></html>');
await page.addScriptTag({path: 'node_modules/mermaid/dist/mermaid.min.js'});
const svg = await page.evaluate(async (src, layout, id) => {
  mermaid.initialize({startOnLoad:false, layout, handDrawnSeed:1});
  const r = await mermaid.render(id, src);
  return r.svg;
}, fs.readFileSync(mmd,'utf8'), layout, id);
fs.writeFileSync(out, svg);
await browser.close();
