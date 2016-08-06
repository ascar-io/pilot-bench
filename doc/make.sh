#!/bin/bash
# Grab pages from Wiki and make

cd `dirname $0`
. config

pushd "${PILOT_WIKI_DIR}"
git pull
popd

cp ${PILOT_WIKI_DIR}/Requirements-and-Installation-Instructions.md .
cp ${PILOT_WIKI_DIR}/Measuring-the-duration-of-running-CPP-functions.md tutorials
cp ${PILOT_WIKI_DIR}/Using-Pilot-to-run-a-command-line-benchmark-job.md tutorials
make html
