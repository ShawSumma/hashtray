#!/bin/bash
# Run the simulation multiple times, for varying values of PercentageGoodConnections (the variable PGC below).
# Nik Sultana, UPenn, January 2018
#
# Usage: ./gen_data.sh -h 35 | tee without_pchast.data_I1
#        ./gen_data.sh -h 35 -p | tee with_pchast.data_I1

EXT_PARAMS=$@

echo "# $@"
echo "PGC Dmin Davg Dmax"
for PGC in `seq 0 1 100`
do
  for ITERATION in `seq 1 5`
  do
    ./garnish_multithreaded -g ${PGC} $@
    # FIXME check exit status
  done | awk -v pgc="${PGC}" \
    'BEGIN { min = max = "nan"; sum = 0}
     { if (NR == 1) min = max = $2;
       if ($2 < min) min = $2;
       if ($2 > max) max = $2;
       sum += $2 }
     END { avg = sum / NR;
           print pgc" "min" "avg" "max }'
done
