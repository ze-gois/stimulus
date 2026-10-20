// Run after compiling: node tests/web_smoke.cjs (server on http://127.0.0.1:8000).
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
(async () => {
  const browser = await chromium.launch({headless:true, args:['--enable-webgl', '--use-gl=angle', '--use-angle=swiftshader']});
  try {
    const page = await browser.newPage({viewport:{width:1000,height:1200}, acceptDownloads:true});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('http://127.0.0.1:8000');
    await page.waitForFunction(() => document.getElementById('status').textContent.startsWith('Ready'));
    const canvas = page.locator('canvas');
    async function click(x,y) { const box = await canvas.boundingBox(); await page.mouse.click(box.x+x*box.width/800,box.y+y*box.height/800); }
    async function importLibrary(value) {
      await page.locator('#import').setInputFiles({name:'library.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(value))});
      await page.waitForFunction(() => document.getElementById('status').textContent.startsWith('Imported'));
    }
    const config = {type:'Fixing',duration:1,repetitions:2};
    const library = {schema_version:1,stimuli:[config],sequence:[config]};
    // Empty libraries must remain usable; create a stimulus through the editor.
    await importLibrary({schema_version:1,stimuli:[],sequence:[]});
    await click(70,600); await click(490,565); // New, Apply
    await click(80,648); // Save
    assert.equal(await page.evaluate(() => JSON.parse(localStorage.getItem('stimulus.library.v1')).stimuli.length),1);
    await importLibrary(library);
    await click(80,648);
    await page.reload();
    await page.waitForFunction(() => document.getElementById('status').textContent.startsWith('Ready'));
    await canvas.focus();
    await click(700,600); // Start
    await page.waitForTimeout(250);
    await page.keyboard.press('a');
    await page.waitForTimeout(2300);
    const downloadPromise = page.waitForEvent('download');
    await click(100,415);
    const download = await downloadPromise;
    const report = JSON.parse(await fs.readFile(await download.path(),'utf8'));
    assert.equal(report.status,'completed');
    assert.equal(report.trials.length,2);
    assert.equal(report.trials[0].responses[0].key,65);
    assert.ok(report.trials.every(t => t.end_seconds-t.onset_seconds >= 1));
    assert.ok(report.trials[0].responses[0].trial_seconds < 1);
    await click(580,415); // Back
    await click(700,600); await page.waitForTimeout(100); await page.keyboard.press('Escape');
    const abortedDownload = page.waitForEvent('download'); await click(100,415);
    const aborted = JSON.parse(await fs.readFile(await (await abortedDownload).path(),'utf8'));
    assert.equal(aborted.status,'aborted_escape');
    assert.equal(aborted.trials.length,1);
    // Failed import is rejected without losing the valid saved library.
    await page.locator('#import').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from('{bad')});
    await page.waitForFunction(() => document.getElementById('status').textContent.startsWith('Import rejected'));
    assert.equal(await page.evaluate(() => JSON.parse(localStorage.getItem('stimulus.library.v1')).sequence.length),1);
    await page.screenshot({path:'web-smoke.png',fullPage:true});
    assert.deepEqual(errors,[]);
    console.log('Web smoke passed: empty library, editor, persistence, import, presentation, responses, export, abort.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode=1; });
