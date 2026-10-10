# Reference Documents

Datasheets and application notes used as the technical basis for this firmware

## Si473x Programming Guides

- `PDF/AN332(1.2a).pdf`
  Si47XX Programming Guide, Rev 1.2a
  Primary API reference for the Si4735 command set (power up, tuning, properties, RDS, seek)
  Cited throughout `SI4735.cpp` / `SI4735.h` as "Si47XX PROGRAMMING GUIDE; AN332"

- `PDF/AN332(0.8).pdf`
  Earlier revision of the same programming guide (Rev 0.8)
  Some SSB/NBFM details differ between revisions

## SSB / NBFM Patch Amendment

- `PDF/si4735_SSB_NBFM(Rev. 0.8a).pdf`
  AN332 Rev 0.8 Universal Programming Guide, Amendment for Si4735-D60 SSB and NBFM patches
  Defines the patch download procedure (POWER_UP with PATCH enable, 0x15 / 0x16 rows)
  Basis for `patch_ssb_new.h`, `patch_ssb_old.h`, `patch_am.h` and `downloadCompressedPatch()`

## Display

- `PDF/SSD1306.pdf`
  SSD1306 OLED controller datasheet
  Basis for the I2C OLED driver used by the UI (`SSD1306_OLED.h`)
