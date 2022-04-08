#!/bin/bash
# Simple wrapper script to perform Coverity scan on Procster (using make default target)
# ## Refs
# https://community.synopsys.com/s/article/Is-it-possible-to-get-the-issue-categorization-e-g-high-impact-medium-by-running-cov-analyze-only
# https://community.synopsys.com/s/article/Coverity-PW-checker
# COVINSTPATH=/usr/local/cov_2020.09/Linux-64/
if [ -z $COVINSTPATH ]; then
  echo "No COVINSTPATH (Coverity installation path) given !"; exit 1
fi
export PATH="$COVINSTPATH/bin/:$PATH"
CHECK_ENADISA=
MODELFILE=covint/covmodels.xmldb
PW_CFG=
# --disable CHECKED_RETURN
mkdir covint
# cp $COVINSTPATH/config/parse_warnings.conf.sample ./covint/parse_warnings.conf
echo "chk \"PW.ASSIGN_WHERE_COMPARE_MEANT\": off;" > ./covint/parse_warnings.conf
# Compile model
cov-make-library --output-file covint/covmodels.xmldb covmodels.c
cov-configure --config covint/coverity_config.xml --gcc

cov-build --dir covint --config covint/coverity_config.xml make
# cov-manage-emit ...
# --checker-option / -co
# Use model:
MOD="--user-model-file covint/covmodels.xmldb"
# --disable PW.ASSIGN_WHERE_COMPARE_MEANT
cov-analyze --dir covint --strip-path `pwd`  $MOD --aggressiveness-level high --enable-parse-warnings --parse-warnings-config ./covint/parse_warnings.conf --disable DEADCODE
cov-format-errors --dir covint --json-output-v7 covint/errors.json

