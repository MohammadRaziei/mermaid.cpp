// Dumps the resolved theme variables of a Mermaid theme with Mermaid's own theme code.
// Used to fill src/theme_variables.cpp (values are not re-derived in C++).
//
//   git clone --depth 1 https://github.com/mermaid-js/mermaid && git -C mermaid checkout mermaid@12.2.0
//   npm i khroma esbuild            (in an empty scratch folder, NOT in this repo)
//   npx esbuild <mermaid>/packages/mermaid/src/themes/theme-redux-color.js --bundle --platform=node --format=cjs --outfile=theme.cjs
//   node dump_theme_variables.mjs theme.cjs
//
// Prints the variables the flowchart stylesheet reads, as JSON.
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const theme = require(process.argv[2] ?? './theme.cjs').getThemeVariables();
const keys = ['fontFamily', 'fontSize', 'textColor', 'errorBkgColor', 'errorTextColor', 'lineColor', 'nodeTextColor', 'mainBkg',
  'nodeBorder', 'strokeWidth', 'arrowheadColor', 'edgeLabelBackground', 'clusterBkg', 'clusterBorder', 'titleColor',
  'tertiaryColor', 'border2', 'THEME_COLOR_LIMIT', 'background', 'borderColorArray', 'bkgColorArray', 'dropShadow', 'useGradient'];
console.log(JSON.stringify(Object.fromEntries(keys.map((k) => [k, theme[k]])), null, 2));
