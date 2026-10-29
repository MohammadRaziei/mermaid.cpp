# ADR-0016: Compute the themes the way Mermaid does, with khroma's colour arithmetic ported

- Status: Accepted
- Realised in: the commit that added `src/khroma.*` and rewrote `src/theme_variables.cpp` (S7k, S11); replaces the stored
  constants of ADR-0009's theme variables

## Context

Until now every theme (`default`, `dark`, `forest`, `neutral`, `base`) was a table of final values copied from Mermaid
12.2.0, and only `themeVariables.fontFamily`, `clusterBkg` and `clusterBorder` could be overridden. In Mermaid a
`themeVariables` override goes to the theme class's `calculate(overrides)`: the overrides are assigned, `updateColors()`
derives what is missing (`secondaryColor = adjust(primaryColor, {h: -120})`, `edgeLabelBackground`, the gradient stops, ...)
and the overrides are assigned again. A single `primaryColor` therefore changes many colours, by khroma's HSL arithmetic,
written with khroma's number formatting (`hsl(40.5882352941, 60%, 83.3333333333%)`).

Asking Mermaid itself (a probe page that reads `getConfig().themeVariables`) showed three more rules for a `%%{init}%%`
directive: `sanitizeDirective` drops every key that is not in the default config (of the variables our themes read, these
are `mainContrastColor`, `contrast`, `text`, `nodeTextColor`, `nodeShadow`), empties string values that hold characters
other than digits, space, quotes and `#%(),.;` and letters, and a plain `fontFamily` becomes `themeVariables.fontFamily`.

## Decision

- `src/khroma.*` ports the khroma 2.1 pieces the themes use (parse of hex, `rgb()`, `hsl()` and keywords, the lazy RGB/HSL
  channels, `adjust`, `lighten`, `darken`, `invert`, `mix`, `rgba`, luminance, `isDark`) and works on colour strings, so a
  derived colour is the same text as in Mermaid.
- `src/theme_variables.cpp` holds, per theme, the constructor and `updateColors()` statements that matter for the flowchart,
  in Mermaid's order, on a small string bag with JavaScript's `a = a || b` (`theme_state::otherwise`, lazy so a bad colour
  only throws when it is needed). `calculate` assigns the overrides, updates, assigns again; `base` also turns the gradient
  off when `nodeBorder` is given and `useGradient` is not.
- `flowchart::InitConfig` keeps every directive `themeVariables` entry (`theme_variables`, strings, numbers as JavaScript
  prints them, `true` / empty for booleans) after the sanitising rules above; the config-level `fontFamily` is kept apart
  because it is what the `:root` rule reads.

## Consequences

- The defaults of all five themes are computed, and the 10 theme fixtures plus 19 override fixtures (`t_*`) agree with
  Mermaid; the stored tables are gone, so a mistake in a derivation shows up in those fixtures.
- Only the variables the flowchart reads are modelled. A theme variable read by another diagram type is ignored.
- `look`, `layout` and `htmlLabels` are still not read from `init`.
- A font family that the pinned test fonts do not contain (`monospace`) cannot be checked against the reference.

## Alternatives considered

- Transpiling Mermaid's theme classes mechanically: too large for the few variables used.
- Keeping tables and deriving only `primaryColor`: wrong as soon as another variable is overridden.
