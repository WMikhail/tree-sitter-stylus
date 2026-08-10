# tree-sitter-stylus

A [`tree-sitter`](https://tree-sitter.github.io/tree-sitter/) grammar for the
[Stylus](https://stylus-lang.com/) stylesheet language.

## Supported syntax

- indentation-sensitive and brace-delimited blocks
- tag, class, ID, placeholder, attribute, pseudo, universal, and nested selectors
- declarations, nested property groups, assignments, and custom properties
- numeric values, units, colors, strings, lists, ranges, casts, and operators
- function definitions, calls, default, named, and rest arguments, and CSS functions
- selector, property, value, media, and keyframe interpolation
- `if`, `unless`, postfix conditions and loops, `each`, `for`, `while`, and `return`
- imports, generic at-rules, media and supports queries, font faces, keyframes, and extends
- block values with `@block` and literal CSS with `@css`
- line and block comments

The repository includes highlight and symbol-tag queries, Node and Rust
bindings, generated parser sources, and a corpus covering indentation, selectors,
declarations, expressions, control flow, and at-rules.

## Development

Install the pinned Tree-sitter CLI and run the corpus:

```sh
npm ci
npm run check
```

Parse a file or inspect query matches with:

```sh
npx tree-sitter parse path/to/file.styl
npx tree-sitter query queries/highlights.scm path/to/file.styl
```

Generated files in `src/` are committed. After changing `grammar.js` or
`src/scanner.c`, run `npm run generate` and commit the resulting parser files.

## Limitations

Stylus accepts highly flexible expressions and selector interpolation. Uncommon
combinations may require additional grammar rules; contributions should include
focused corpus cases that demonstrate the expected syntax tree.

## License

Licensed under the [MIT License](LICENSE).
