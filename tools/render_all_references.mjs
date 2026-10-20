import chromium from '@sparticuz/chromium';
import puppeteer from 'puppeteer-core';
import fs from 'fs';
const [,, dir, out, layout='dagre'] = process.argv;
// what mermaid-cli adds to the serialised svg: xmlns:xlink and the background colour
function mmdcLike(svg) {
  return svg.replace(' xmlns="http://www.w3.org/2000/svg"', ' xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"')
            .replace(/style="(max-width: [^"]*?);?"/, 'style="$1; background-color: white;"');
}
const browser = await puppeteer.launch({args: chromium.args, executablePath: await chromium.executablePath(), headless: 'shell'});
for (const f of fs.readdirSync(dir).filter(f=>f.endsWith('.mmd'))) {
  const id = f.replace(/\.mmd$/,'');
  const page = await browser.newPage();
  try {
    await page.setContent('<html><body></body></html>');
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
