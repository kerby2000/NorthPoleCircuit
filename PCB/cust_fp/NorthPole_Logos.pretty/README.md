# NorthPole_Logos.pretty

Decorative silkscreen-only KiCad footprints for wireless markings on the North Pole postcard PCB.

These footprints contain no pads, no nets, no copper artwork, and no solder-mask openings. They are marked `board_only exclude_from_pos_files exclude_from_bom allow_missing_courtyard`, so they are intended for board graphics only and should not appear in BOM or CPL/position outputs.

## Footprints

| Footprint | Source/reference | Target envelope | Actual artwork box | Artwork layer | Notes |
| --- | --- | --- | --- | --- | --- |
| `Bluetooth_Logo_5.0x5.0mm_SilkScreen` | `bluetooth-icon-seeklogo-3.svg` plus cleaned KiCad-imported body polygons | 5.0 mm x 5.0 mm | 5.00 mm x 3.88 mm | `F.SilkS` | Direct-import style filled polygons; registration mark removed; short large-radius rounded signal arcs. |
| `Bluetooth_Logo_6.0x6.0mm_SilkScreen` | `bluetooth-icon-seeklogo-3.svg` plus cleaned KiCad-imported body polygons | 6.0 mm x 6.0 mm | 5.75 mm x 4.61 mm | `F.SilkS` | Direct-import style filled polygons; registration mark removed; compact short large-radius rounded signal arcs. |
| `WiFi_Logo_6.0x5.0mm_SilkScreen` | `WiFi_Logo.svg` imported into KiCad, ungrouped, then cleaned with polygon subtraction | 6.0 mm x 5.0 mm | 5.98 mm x 3.88 mm | `F.SilkS` | Four cleaned body polygons plus short large-radius rounded side signal arcs; TM mark and old duplicate artwork removed. |
| `WiFi_Logo_7.0x5.75mm_SilkScreen` | `WiFi_Logo.svg` imported into KiCad, ungrouped, then cleaned with polygon subtraction | 7.0 mm x 5.75 mm | 6.98 mm x 4.53 mm | `F.SilkS` | Four cleaned body polygons plus short large-radius rounded side signal arcs; TM mark and old duplicate artwork removed. |

`Electrical_admin_btx2tbw6_WiFi-Blocks.dwg` was not required for geometry generation; keep it as optional visual/reference material only.

## Manufacturability

The source SVGs include small registration/trademark text that is too fine for small PCB silkscreen. Those marks were removed during conversion. The remaining source artwork is represented as direct-import style filled `fp_poly` primitives and centered at the footprint origin. Signal arcs are generated as large-radius annular filled polygons with rounded ends and mirrored about the horizontal axis. Verify the chosen board house's latest silkscreen limits before release.

## Usage

The project-local footprint library is registered as `NorthPole_Logos` in `PCB/fp-lib-table`.

In KiCad:

1. Use **Place Footprint**.
2. Select `NorthPole_Logos`.
3. Place one of:
   - `NorthPole_Logos:Bluetooth_Logo_5.0x5.0mm_SilkScreen`
   - `NorthPole_Logos:Bluetooth_Logo_6.0x6.0mm_SilkScreen`
   - `NorthPole_Logos:WiFi_Logo_6.0x5.0mm_SilkScreen`
   - `NorthPole_Logos:WiFi_Logo_7.0x5.75mm_SilkScreen`
4. Keep the footprint on the front side for `F.SilkS`, or flip it if a back-side silkscreen logo is desired.

Verify trademark and licensing requirements before using official Bluetooth or Wi-Fi marks on public or commercial hardware.
