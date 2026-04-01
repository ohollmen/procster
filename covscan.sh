#!/bin/bash
# Simple wrapper script to perform Coverity scan on Procster (using make default target)
# ## Refs
# https://community.synopsys.com/s/article/Is-it-possible-to-get-the-issue-categorization-e-g-high-impact-medium-by-running-cov-analyze-only
# https://community.synopsys.com/s/article/Coverity-PW-checker
# COVINSTPATH=/usr/local/cov_2020.09/Linux-64/
# Note: Converted to use intermediate directory name "cov-int" (not covint) as https://scan.coverity.com
# seems to be strict about this.
if [ -z $COVINSTPATH ]; then
  echo "No COVINSTPATH (Coverity installation path) given !"; exit 1
fi
export PATH="$COVINSTPATH/bin/:$PATH"
CHECK_ENADISA=
MODELFILE=cov-int/covmodels.xmldb
PW_CFG=
# --disable CHECKED_RETURN
mkdir -p cov-int
# cp $COVINSTPATH/config/parse_warnings.conf.sample ./cov-int/parse_warnings.conf
echo "chk \"PW.ASSIGN_WHERE_COMPARE_MEANT\": off;" > ./cov-int/parse_warnings.conf

# Also (e.g.): cov-configure --comptype gcc --compiler /usr/bin/mygcc-4.6
cov-configure --config cov-int/coverity_config.xml --gcc
# Run Build (check tail cov-int/build-log.txt after)
cov-build --dir cov-int --config cov-int/coverity_config.xml make all2
# cov-manage-emit is still included in the scan.coverity.com version.
# cov-manage-emit ...
# --checker-option / -co
command -v cov-analyze >/dev/null 2>&1 || { echo "Utility cov-analyze missing (probably using Coverity Scan build-only package)"; exit 0; }
# Both cov-analyze and cov-format-errors missing from scan.coverity.com
# downloadable version => upload tar.gz:ed cov-int to scan.coverity.com.
# Compile model (covmodels.c => conf/covmodels.c)
# Even cov-make-library is missing in scan.coverity.com version !!!
cov-make-library --output-file cov-int/covmodels.xmldb conf/covmodels.c
# Use model:
MOD="--user-model-file cov-int/covmodels.xmldb"
# --disable PW.ASSIGN_WHERE_COMPARE_MEANT
cov-analyze --dir cov-int --strip-path `pwd`  $MOD --aggressiveness-level high --enable-parse-warnings --parse-warnings-config ./cov-int/parse_warnings.conf --disable DEADCODE
cov-format-errors --dir cov-int --json-output-v7 cov-int/errors.json
