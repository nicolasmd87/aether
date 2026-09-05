#!/usr/bin/env sh
set -e
localDir=$(dirname "$0")
node build.js "$localDir/tests" > "$localDir/cts.json"
