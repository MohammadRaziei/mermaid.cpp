// Generates the C++ factories of src/theme_variables.cpp (dark, forest, neutral, base) from Mermaid 12.2.0 theme code.
//   sparse-clone mermaid@12.2.0 (packages/mermaid/src/themes), npm i khroma esbuild in a scratch folder, then for each theme:
//   NODE_PATH=<scratch>/node_modules npx esbuild <mermaid>/packages/mermaid/src/themes/theme-<name>.js --bundle --platform=node --format=cjs --outfile=theme-<name>.cjs
//   node gen_theme_variables.cjs   (prints the C++; paste it into src/theme_variables.cpp)
const q = (s) => JSON.stringify(s);
const arr = (a) => (a && a.length ? '{' + a.map(q).join(', ') + '}' : '{}');
let out = '';
for (const [fn, file] of [['dark', 'dark'], ['forest', 'forest'], ['neutral', 'neutral'], ['base', 'base']]) {
  const v = require(`./theme-${file}.cjs`).getThemeVariables();
  out += `theme_variables theme_variables::${fn}() {\n    theme_variables t;\n`;
  const set = (m, val) => { out += `    t.${m} = ${val};\n`; };
  set('font_family', q(v.fontFamily)); set('font_size', q(v.fontSize)); set('text_color', q(v.textColor));
  set('error_bkg_color', q(v.errorBkgColor)); set('error_text_color', q(v.errorTextColor)); set('line_color', q(v.lineColor));
  set('node_text_color', q(v.nodeTextColor ?? v.textColor ?? '')); set('main_bkg', q(v.mainBkg)); set('node_border', q(v.nodeBorder));
  set('stroke_width', v.strokeWidth); set('arrowhead_color', q(v.arrowheadColor)); set('edge_label_background', q(v.edgeLabelBackground));
  set('cluster_bkg', q(v.clusterBkg)); set('cluster_border', q(v.clusterBorder)); set('title_color', q(v.titleColor));
  set('tertiary_color', q(v.tertiaryColor)); set('border2', q(v.border2)); set('drop_shadow', q(v.dropShadow ?? ''));
  set('use_gradient', v.useGradient ? 'true' : 'false'); set('gradient_start', q(v.gradientStart ?? '')); set('gradient_stop', q(v.gradientStop ?? ''));
  set('theme_color_limit', v.THEME_COLOR_LIMIT ?? 0);
  if (v.borderColorArray) set('border_color_array', arr(v.borderColorArray));
  if (v.bkgColorArray) set('bkg_color_array', arr(v.bkgColorArray));
  set('font_family_config', q('"trebuchet ms", verdana, arial, sans-serif'));
  out += `    return t;\n}\n\n`;
}
console.log(out);
