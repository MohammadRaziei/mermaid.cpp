import chromium from '@sparticuz/chromium';
import puppeteer from 'puppeteer-core';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
const [,, dir, out, layout='dagre'] = process.argv;
// what mermaid-cli adds to the serialised svg: xmlns:xlink and the background colour, and the XML form of <br>
// (it serialises the DOM with XMLSerializer, mermaid.render() returns the HTML string)
function mmdcLike(svg) {
  return svg.replace(/<br>/g, '<br />').replace(' xmlns="http://www.w3.org/2000/svg"', ' xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"')
            .replace(/style="(max-width: [^"]*?);?"/, 'style="$1; background-color: white;"');
}
// Text widths come from the font, so the font is pinned: Liberation Sans (tests/fonts, SIL OFL) stands in for every
// family Mermaid asks for, in the four styles Markdown labels use. It is the font the metric tables were measured with.
const fontDir = process.env.MERMAID_REFERENCE_FONT_DIR ||
  path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'tests', 'fonts');
const faces = [['Regular', 'normal', 'normal'], ['Bold', 'bold', 'normal'], ['Italic', 'normal', 'italic'], ['BoldItalic', 'bold', 'italic']];
const fontCss = ['arial', 'trebuchet ms', 'verdana', 'Recursive Variable', 'sans-serif', 'Helvetica', 'Arial'].flatMap(family =>
  faces.map(([file, weight, style]) =>
    `@font-face{font-family:"${family}";font-weight:${weight};font-style:${style};src:url(data:font/ttf;base64,` +
    `${fs.readFileSync(path.join(fontDir, `LiberationSans-${file}.ttf`)).toString('base64')});}`)).join('');
const browser = await puppeteer.launch({args: chromium.args, executablePath: await chromium.executablePath(), headless: 'shell'});
for (const f of fs.readdirSync(dir).filter(f=>f.endsWith('.mmd'))) {
  const id = f.replace(/\.mmd$/,'');
  const page = await browser.newPage();
  try {
    await page.setContent('<html><head><style>' + fontCss + '</style></head><body></body></html>');
    await page.evaluate(async () => { await Promise.all([...document.fonts].map(f => f.load())); await document.fonts.ready; });  // measure only once the pinned font is in
    await page.addScriptTag({path: 'node_modules/mermaid/dist/mermaid.min.js'});
    const svg = await page.evaluate(async (src, layout, id) => {
      mermaid.initialize({startOnLoad:false, layout, handDrawnSeed:1});
      return (await mermaid.render(id, src)).svg;
    }, fs.readFileSync(dir+'/'+f,'utf8'), layout, id);
    fs.writeFileSync(out+'/'+id+'.svg', mmdcLike(svg));
  } catch (e) { console.log('FAIL', id, String(e).slice(0,100)); }
  await page.close();
}
await browser.close();
