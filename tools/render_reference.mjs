import chromium from '@sparticuz/chromium';
import puppeteer from 'puppeteer-core';
import fs from 'fs';
const [,, mmd, out, layout='dagre', id='basic'] = process.argv;
// what mermaid-cli adds to the serialised svg: xmlns:xlink and the background colour, and the XML form of <br>
// (it serialises the DOM with XMLSerializer, mermaid.render() returns the HTML string)
function mmdcLike(svg) {
  return svg.replace(/<br>/g, '<br />').replace(' xmlns="http://www.w3.org/2000/svg"', ' xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"')
            .replace(/style="(max-width: [^"]*?);?"/, 'style="$1; background-color: white;"');
}
const browser = await puppeteer.launch({args: chromium.args, executablePath: await chromium.executablePath(), headless: 'shell'});
const page = await browser.newPage();
await page.setContent('<html><body><div id="c"></div></body></html>');
await page.addScriptTag({path: 'node_modules/mermaid/dist/mermaid.min.js'});
const svg = await page.evaluate(async (src, layout, id) => {
  mermaid.initialize({startOnLoad:false, layout, handDrawnSeed:1});
  const r = await mermaid.render(id, src);
  return r.svg;
}, fs.readFileSync(mmd,'utf8'), layout, id);
fs.writeFileSync(out, mmdcLike(svg));
await browser.close();
