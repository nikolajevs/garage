#!/bin/bash
# Экспорт гербер-файлов и файлов сверловки для заказа на JLCPCB.
# Требует KiCad 7+ (kicad-cli в PATH).
# Запускать ПОСЛЕ разводки дорожек и прохождения DRC!
set -e
cd "$(dirname "$0")"
OUT="jlcpcb_gerbers"
rm -rf "$OUT" && mkdir -p "$OUT"

# Слои, которые нужны JLCPCB для 2-слойной платы
kicad-cli pcb export gerbers \
  --output "$OUT/" \
  --layers F.Cu,B.Cu,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts \
  --subtract-soldermask \
  garage_carrier.kicad_pcb

# Файлы сверловки: Excellon, метрические, PTH и NPTH раздельно (JLC понимает оба варианта)
kicad-cli pcb export drill \
  --output "$OUT/" \
  --format excellon \
  --drill-origin absolute \
  --excellon-units mm \
  --generate-map \
  --map-format gerberx2 \
  garage_carrier.kicad_pcb

# Упаковка: JLCPCB принимает один zip
zip -j "garage_carrier_jlcpcb.zip" "$OUT"/*
echo ""
echo "Готово: garage_carrier_jlcpcb.zip — загружайте на cart.jlcpcb.com"
