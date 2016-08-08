#!/bin/bash
# Grab pages from Wiki and make
set -e -u
cd `dirname $0`
if [ ! -f config ]; then
    echo <<EOF
Error: cannot find Pilot Wiki source. This documentation needs files from the
Pilot Wiki. Please clone https://github.com/ascar-io/pilot-bench.wiki.git
locally, add its path to config.template, and save the result file to config.
EOF
    exit 1
fi
. config

pushd "${PILOT_WIKI_DIR}"
git pull
popd

cp ${PILOT_WIKI_DIR}/Requirements-and-Installation-Instructions.rest .
cp ${PILOT_WIKI_DIR}/Measuring-the-duration-of-running-CPP-functions.md tutorials
cp ${PILOT_WIKI_DIR}/Using-Pilot-to-run-a-command-line-benchmark-job.md tutorials
make html
