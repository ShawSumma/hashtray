#!/bin/bash
# Run gen_data.sh for varying values of PercentageGoodHosts (the variable PGH below).
# This produces an output that is then graphed using the graph.R script.
# Nik Sultana, UPenn, January 2018
#
# Usage: ./sweep.sh

for PGH in `seq 1 5 100`
do
  echo "PGH=${PGH}"
  ./gen_data.sh -h ${PGH} | tee without_pchast.data_O1_units_${PGH}
  ./gen_data.sh -h ${PGH} -p | tee with_pchast.data_O1_units_${PGH}
done
