# Stillmar Electric

A plain, static website for stillmar electric, live at https://stillmar.com. It has Home and About pages, with no listings, sales, forms, external assets, or JavaScript.

## Local preview

From this directory, run:

```sh
python3 -m http.server 8080 --bind 127.0.0.1 --directory dist
```

Open `http://localhost:8080/`. Stop the server with Ctrl+C. The relative links also allow `dist/index.html` to open directly from disk.

## Editing

The HTML files in `dist/` are the source pages, with `index.html` as Home. `dist/styles.css` owns the shared styles. Edit these files directly; there is no build step.

Each page includes the same navigation links, with `aria-current="page"` on its own link. Keep the links consistent when adding or renaming pages. Check navigation between all pages, text, and layout at desktop and narrow mobile widths after changes.

Each page's `<head>` has a description, a canonical URL, favicon links, and Open Graph tags for link previews. When adding a page, copy them and update the title, description, canonical URL, and `og:` values. Give every `<img>` `width` and `height` attributes matching the image file, so the layout doesn't jump while it loads.

Keep images small: strip metadata (especially from photos) and downsample before adding them, e.g.

```sh
magick photo.jpg -auto-orient -resize 1600x -strip -quality 82 dist/images/photo.jpg
```

## Deploying

The site is hosted on Cloudflare, in the Stilmar Electric account, as a Worker that serves `dist/` as static files. `nix-shell` (or direnv) provides `wrangler` and `just`. Log in once with `wrangler login`, then:

```sh
just deploy               # publish dist/ to stillmar.com
just deploy-www-redirect  # publish the www.stillmar.com -> stillmar.com redirect
```

`wrangler.toml` configures the site. `www-redirect/` is a separate small Worker that 301-redirects every `www.stillmar.com` URL to `stillmar.com`; it only needs redeploying if it changes.

Cloudflare serves `index.html` at `/` and redirects `.html` URLs to clean ones (`/about.html` → `/about`).
