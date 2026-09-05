chcp 65001

$localDir=$PSScriptRoot
node build.js "$localDir/tests" > "$localDir/cts.json"
