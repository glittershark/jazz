# Website instructions

This site is live at https://stillmar.com. Develop and preview it locally. Do not run `just deploy`, `wrangler deploy`, or change Cloudflare settings unless the user explicitly asks for that deploy; earlier approval does not carry over. Repository storage and pull requests do not authorize deployment.

Keep the site simple, with no product listings until requested. Add product details, contact addresses, and social links only when provided; do not invent them.

The HTML files in `dist/` are the canonical pages, and `dist/styles.css` owns shared styles. No framework or build step is needed. Keep the navigation links consistent across pages and mark the current page with `aria-current="page"`. Keep links relative so pages still open from disk.

When adding or changing pages, keep each page's `<head>` tags (description, canonical URL, favicon links, Open Graph tags) accurate, and give every `<img>` `width`/`height` attributes matching the file plus meaningful `alt` text (`alt=""` for decorative images). Strip metadata from and downsample any new images before adding them.

See `README.md` for preview and deploy commands.
