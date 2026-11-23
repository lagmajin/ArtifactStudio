# Middlebury ColorChecker 24 R&D subset

**最終更新:** 2026-11-16

Source page: [Middlebury Color Datasets](https://vision.middlebury.edu/color/data/)

The dataset page describes registered views of the X-Rite 24-patch ColorChecker
photographed with 35 camera models, under tungsten/daylight illumination and
multiple exposures. It states that the RAW files were rendered to standard
linear RGB PNG with dcraw using camera white-balance multipliers, and that all
images on the site may be used and published with citation to Chakrabarti,
Scharstein, and Zickler, “An empirical camera model for Internet color vision,”
BMVC 2009. The page does not call this a CC0 license; this R&D subset preserves
the attribution and source hashes.

The archive was downloaded for inspection to a temporary directory and is not
stored in the repository: `checker24-RAW-JPG.zip`, 128,651,408 bytes,
SHA-256 `e0f58600116121406004717a1dbc1d4e41296f7fac615005988400945a8a3990`.

Selected pair: Canon EOS 20D, `wb1i2e3` (fixed tungsten WB under daylight
illumination, exposure e3 / nominal 0 EV) as source and `wb2i2e3` (auto WB, same
illumination/exposure) as reference. Both are 606×404 registered chart images.
WB2 is only the camera's auto-WB rendition, not calibrated ground truth.

| File | Role | SHA-256 |
|---|---|---|
| `CanonEOS20D/wb1i2e3-jpg.png` | WB1 source JPEG rendition | `4c94a737fbfb9bd860df0a4d1b85d29e1c4a2577c8c600734d2dc6f5e0479199` |
| `CanonEOS20D/wb2i2e3-jpg.png` | WB2 target JPEG rendition | `1e907b7eabb2dfdc7d679b0a187c8f10b798fd1a13757e4fbede61de4cbfad19` |
| `CanonEOS20D/wb1i2e3-raw.png` | WB1 RAW rendering control; standard linear RGB per source page | `050923d0882ad2a6242a25f2ee1be1f7f303db7e007425f56eb7eeb7912e4c98` |
| `CanonEOS20D/wb2i2e3-raw.png` | WB2 RAW rendering control; standard linear RGB per source page | `91f4f28a8948df03680a3db85ebc293353bfc944ce5d18dc417b06fb9cdd505c` |

R&D ROI layout uses the chart's 6 columns × 4 rows. Central 60×60 pixels are
measured from each 101×101 patch. Skin-tone proxy patches are row 0 / columns
0–1; white is row 3 / column 0; neutral grays are row 3 / columns 1–5. These
are standardized chart colors, not human skin or natural white materials.


## Registered natural-scene RAW/JPEG control

Source page: [Middlebury Color Datasets](https://vision.middlebury.edu/color/data/)

The page describes 85 registered RAW/JPG natural-scene pairs across multiple
camera models. RAW images were rendered to standard linear RGB PNG with dcraw
using camera white-balance multipliers; scaled versions reduce noise and JPEG
compression. The page grants permission to use and publish site images with
citation to Chakrabarti et al. (BMVC 2009). The `other-scaled.zip` archive was
inspected temporarily and is not stored in this repository; its SHA-256 is
`6fce6384c086a8088de08497038ab3681dabafe22b0f49832b45961e1a882d98`.

Selected scene: `CanonEOS5D/scaled-im00`, a warm-lit kitchen interior. The RAW
PNG is the source after standard linear-RGB-to-sRGB encoding; the paired camera
JPEG PNG is the reference rendition. The scene is registered, but the JPEG is
not colorimetric ground truth and the transfer may include camera tone/color
processing. Hashes: `scaled-im00-raw.png` =
`cca354ddc2ee8719288e3508b38a50f79587291c03790bab2c7a7e80763f58c2`;
`scaled-im00-jpg.png` =
`fb24747e5a0778a214c13f83a3c4763e6fa77bc01fb6ac721218f086f4963fcf`.

An exploratory near-neutral wall rectangle is stored as an unreviewed candidate
ROI; its actual reflectance/chromaticity is unknown. This photograph has no
skin subject. It is a natural-texture color-matching control, not evidence of
skin or white-object preservation.
