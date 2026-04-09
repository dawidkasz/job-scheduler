#!/usr/bin/env bash
# fetch_temperature_sensor (periodic: sensor A / sensor B)
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080}"

post_job() {
    curl -s -X POST "$BASE_URL/jobs" \
        -H 'Content-Type: application/json' \
        -d "$1"
}

echo "Scheduling sensor A every 5s"
post_job '{
  "kind": "every",
  "name": "fetch_temperature_sensor",
  "everySeconds": 5,
  "body": { "args": { "sensor": "A" } }
}'
echo ""

echo "Scheduling sensor B every 10s"
post_job '{
  "kind": "every",
  "name": "fetch_temperature_sensor",
  "everySeconds": 10,
  "body": { "args": { "sensor": "B" } }
}'
echo ""

echo "Polling GET /jobs (Ctrl+C to stop)"
while true; do
    echo "======================"
    curl -s "$BASE_URL/jobs" | jq -r '.[] | select(.result != null and .result.value != null) | "value: \(.result.value)"'
    sleep 2
done
