HTML syntax highlighting: TS vs Legacy comparison report
=========================================================

Sample file: `tests/syntax/samples/html.html`
Legacy reference: `misc/syntax/html.syntax`
TS query: `misc/syntax-ts/queries-override/html-highlights.scm`
TS colors: `misc/syntax-ts/colors.ini` `[html]`

Aligned with legacy
-------------------

- Tag names (`<html>`, `<body>`, `<div>`, etc.): `brightcyan` - MATCH
- Angle brackets (`<`, `>`, `</`, `/>`): `brightcyan` - MATCH
- Known attribute names (`class`, `id`, `href`, `src`, etc.): `yellow` - MATCH
- Equals sign in attributes: `brightred` - MATCH
- Quoted attribute values: `cyan` - MATCH
- Entity references (`&amp;`, `&lt;`, `&#169;`, etc.): `brightgreen` - MATCH
- Comments (`<!-- -->`): `brown` - MATCH
- DOCTYPE declaration: `brightred` - MATCH
- Closing tags (`</div>`, `</body>`): `brightcyan` - MATCH

Intentional improvements over legacy
-------------------------------------

- TS highlights ALL attribute names uniformly as `yellow` via `@property.key`,
  while legacy only recognizes a fixed list of known attributes per tag (e.g.
  `class`, `id`, `href`). Unrecognized attributes in legacy fall to the tag
  context color (brightcyan). TS correctly colors `data-theme`, `aria-label`,
  `loading`, `placeholder`, `frameborder`, `autoplay`, `muted`, and all custom
  or newer HTML5 attributes.
- TS handles all tags uniformly regardless of tag name, while legacy has
  separate `context` blocks for each known HTML tag. Unknown tags in legacy
  fall to a generic `< >` context colored `cyan` instead of `brightcyan`. TS
  treats `<nav>`, `<section>`, `<article>`, `<header>`, `<footer>`, `<video>`,
  `<iframe>`, `<textarea>`, `<select>`, `<option>`, `<label>`, `<ol>`, `<li>`,
  `<thead>`, `<tbody>`, `<th>`, `<span>`, `<strong>` all consistently as
  `brightcyan`.
- TS properly highlights boolean attributes without values (like `required`,
  `selected`, `controls`, `autoplay`, `muted`) as `yellow`, while legacy
  shows them as part of the tag context color.
- TS handles erroneous/invalid end tags via `@tag` on
  `erroneous_end_tag_name`, showing `</invalidtag>` in `brightcyan`.
- TS correctly handles embedded `<script>` and `<style>` blocks with
  language-specific highlighting (JavaScript/CSS captures apply inside those
  blocks), while legacy treats them as plain text.
- Legacy treats `"_blank"` as a special magenta value and `"post"` as magenta
  only in specific tag contexts. TS treats all attribute values uniformly as
  `cyan`, which is more consistent.

Known shortcomings
------------------

- Legacy distinguishes special URL-like attribute values (e.g. `"http:*"`,
  `"*.gif"`) in `magenta` and target values (`"_blank"`, `"_self"`) in
  `magenta`. TS treats all attribute values uniformly as `cyan` and does not
  differentiate by content.
- Legacy uses `magenta` for special attribute values like `"POST"`, `"GET"`,
  `"text/css"`, `"JavaScript"`. TS does not distinguish these semantic values.
- The TS output for `<style>` blocks triggers embedded CSS grammar highlighting,
  which may be unexpected if one only wants HTML-level coloring.
