// Dumps the resolved theme variables of a Mermaid theme with Mermaid's own theme code.
// The oracle for src/theme_variables.cpp, which derives the variables itself (ADR-0016): dump a theme with some overrides and
// compare with what the C++ computes. Pass overrides as a second argument (a JSON object).
//
//   git clone --depth 1 https://github.com/mermaid-js/mermaid && git -C mermaid checkout mermaid@12.2.0
//   npm i khroma esbuild            (in an empty scratch folder, NOT in this repo)
//   npx esbuild <mermaid>/packages/mermaid/src/themes/theme-redux-color.js --bundle --platform=node --format=cjs --outfile=theme.cjs
//   node dump_theme_variables.mjs theme.cjs '{"primaryColor": "#336699"}'
//
// Prints the variables the flowchart stylesheet reads, as JSON.
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const overrides = process.argv[3] ? JSON.parse(process.argv[3]) : undefined;
const theme = require(process.argv[2] ?? './theme.cjs').getThemeVariables(overrides);
const keys = ['fontFamily', 'fontSize', 'textColor', 'errorBkgColor', 'errorTextColor', 'lineColor', 'nodeTextColor', 'mainBkg',
  'nodeBorder', 'strokeWidth', 'arrowheadColor', 'edgeLabelBackground', 'clusterBkg', 'clusterBorder', 'titleColor',
  'tertiaryColor', 'border2', 'THEME_COLOR_LIMIT', 'background', 'borderColorArray', 'bkgColorArray', 'dropShadow', 'useGradient'];
console.log(JSON.stringify(Object.fromEntries(keys.map((k) => [k, theme[k]])), null, 2));
