const fs = require('fs');
const os = require('os');
const path = require('path');
const fetch = require('node-fetch');
const { execSync } = require('child_process');

const cmd = process.argv[2] || 'install';
const prefix = execSync('npm prefix -g').toString().trim();
const binDir = path.join(prefix, 'bin');
const dest = path.join(binDir, 'cjsh');

async function getLatestAsset() {
  const res = await fetch('https://api.github.com/repos/cadenfinley/CJsShell/releases/latest');
  const data = await res.json();
  const platform = os.platform(); // 'darwin', 'linux'
  const assets = data.assets || [];
  // pick matching asset name or fallback
  return assets.find(a => a.name.includes(platform)) || assets[0];
}

async function run() {
  if (cmd === 'uninstall') {
    if (fs.existsSync(dest)) fs.unlinkSync(dest);
    console.log('cjsh uninstalled');
    return;
  }

  const asset = await getLatestAsset();
  const url = asset.browser_download_url;
  const res = await fetch(url);
  if (!fs.existsSync(binDir)) fs.mkdirSync(binDir, { recursive: true });
  const fileStream = fs.createWriteStream(dest, { mode: 0o755 });
  await new Promise((resolve, reject) => {
    res.body.pipe(fileStream).on('finish', resolve).on('error', reject);
  });
  console.log(`cjsh ${cmd}ed successfully`);
}

run().catch(err => {
  console.error(err);
  process.exit(1);
});
