# Mode icons

Thirteen original concept drawings created for this project; no Nextion image,
font, or firmware artwork is included. These SVG/PNG assets and their generator
are provided under the repository's MIT license (see ../../../LICENSE).

Edit `../tools/build_icons.py` and regenerate. Runtime assets are 64 × 64 RGBA
PNGs loaded with openHASP `img` objects at `L:/icons/<mode>.png`. SVGs are source
assets only. The white/blue colors follow the existing mode buttons. Icons are
decoration, not diagrams of safe cutting direction or machine trajectories.

Image IDs are button ID + 100 on page 2; `click:false` lets the existing buttons
handle touch. Confirm click-through and memory use on the real LVGL build.
Offline previews do not validate PNG decoding, filesystem installation, or touch.
