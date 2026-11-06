import chromium from '@sparticuz/chromium';
import puppeteer from 'puppeteer-core';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
const [,, dir, out] = process.argv;
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
    await page.addScriptTag({path: 'node_modules/mermaid/dist/mermaid.min.js'});
    const dump = await page.evaluate(async (src) => {
      mermaid.initialize({startOnLoad:false});
      const d = await mermaid.mermaidAPI.getDiagramFromText(src);
      const db = d.db;
      return {title: db.getDiagramTitle ? db.getDiagramTitle() : '', actors: [...(db.getActors() instanceof Map ? db.getActors() : new Map(Object.entries(db.getActors()))).entries()].map(([k,v])=>({key:k, ...v})),
              createdActors: [...(db.getCreatedActors ? db.getCreatedActors() : [])], destroyedActors: [...(db.getDestroyedActors ? db.getDestroyedActors() : [])],
              boxes: db.getBoxes ? db.getBoxes() : [], messages: db.getMessages(), autonumber: db.showSequenceNumbers ? db.showSequenceNumbers() : null, wrap: db.autoWrap ? db.autoWrap() : null};
    }, fs.readFileSync(path.join(dir, f), 'utf8'));
    fs.writeFileSync(path.join(out, id + '.json'), JSON.stringify(dump, null, 1));
  } catch (e) { console.error(f, String(e).slice(0, 300)); }
  await page.close();
}
await browser.close();
