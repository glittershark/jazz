# Stillmar Electric

A plain, static website with Home, Pedals, Merch, Support, and About pages. No listings, sales, forms, external assets, or JavaScript.

## Local preview

From this directory, run:

```sh
python3 -m http.server 8080 --bind 127.0.0.1 --directory dist
```

Open `http://localhost:8080/`. Stop the server with Ctrl+C. The relative links also allow `dist/index.html` to open directly from disk.

Keep the site local unless hosting is explicitly requested. Adding these files to the repository does not deploy the website.

## Editing

The HTML files in `dist/` are the source pages, with `index.html` as Home. `dist/styles.css` owns the shared spacing and navigation styles. Edit these files directly; there is no build step or dependency installation.

Each page includes the same five navigation links, with `aria-current="page"` on its own link. Keep the links consistent when adding or renaming pages. Check navigation between all pages, text, and layout at desktop and narrow mobile widths after changes.
