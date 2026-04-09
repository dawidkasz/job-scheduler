#!/usr/bin/env bash
# -------------------------------------------------------------------
# Ride Service – GPS sensor pipeline demo
#
# Prerequisites:
#   1. Build the project   – cmake --build build
#   2. Start the server    – ./build/ride_service_example
#   3. Run this script     – bash examples/ride_service/demo.sh
#
# DAG (diamond):
#          FetchGPSData   (stub – random lat,lng near NYC)
#           /          \
#     EstimateETA   CalculateSurge
#           \          /
#        UpdateRideStatus
#
# The script first shows how to schedule FetchGPSData as a periodic
# job, then runs a single end-to-end pipeline with dependencies.
# -------------------------------------------------------------------
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080}"

# -- helpers --------------------------------------------------------

post_job() {
    curl -s -X POST "$BASE_URL/jobs" \
        -H 'Content-Type: application/json' \
        -d "$1"
}

get_job() {
    curl -s "$BASE_URL/jobs/$1"
}

extract_id() {
    echo "$1" | grep -oP '"id"\s*:\s*"\K[^"]+'
}

wait_for_job() {
    local job_id="$1"
    local status=""
    while true; do
        local resp
        resp=$(get_job "$job_id")
        status=$(echo "$resp" | grep -oP '"status"\s*:\s*"\K[^"]+')
        if [[ "$status" == "Completed" || "$status" == "Failed" ]]; then
            echo "$resp"
            return
        fi
        sleep 0.3
    done
}

# -- periodic scheduling (fire-and-forget) -------------------------

echo "=== Periodic scheduling example ==="
echo "    Scheduling FetchGPSData to run every 30 seconds..."
RESP=$(post_job '{
  "kind": "every",
  "name": "fetch_gps_data",
  "everySeconds": 30,
  "body": { "args": {} }
}')
echo "    Server response: $RESP"
echo "    (FetchGPSData will keep running in the background every 30s)"
echo ""

# -- single pipeline run -------------------------------------------

echo "=== Single pipeline run (diamond DAG) ==="

echo "--- 1. FetchGPSData ---"
RESP=$(post_job '{
  "kind": "run",
  "name": "fetch_gps_data",
  "body": { "args": {} }
}')
GPS_ID=$(extract_id "$RESP")
echo "    FetchGPSData id: $GPS_ID"

echo "--- 2. EstimateETA (depends on FetchGPSData) ---"
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"estimate_eta\",
  \"body\": {
    \"args\": { \"dest_lat\": 40.758, \"dest_lng\": -73.9855 },
    \"dependsOn\": [\"$GPS_ID\"]
  }
}")
ETA_ID=$(extract_id "$RESP")
echo "    EstimateETA id: $ETA_ID"

echo "--- 3. CalculateSurge (depends on FetchGPSData) ---"
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"calculate_surge\",
  \"body\": {
    \"args\": {},
    \"dependsOn\": [\"$GPS_ID\"]
  }
}")
SURGE_ID=$(extract_id "$RESP")
echo "    CalculateSurge id: $SURGE_ID"

echo "--- 4. UpdateRideStatus (depends on EstimateETA + CalculateSurge) ---"
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"update_ride_status\",
  \"body\": {
    \"args\": {},
    \"dependsOn\": [\"$ETA_ID\", \"$SURGE_ID\"]
  }
}")
RIDE_ID=$(extract_id "$RESP")
echo "    UpdateRideStatus id: $RIDE_ID"

# -- wait and display results --------------------------------------

echo ""
echo "=== Waiting for pipeline to complete... ==="
echo ""

echo "--- FetchGPSData result ---"
wait_for_job "$GPS_ID"
echo ""

echo "--- EstimateETA result ---"
wait_for_job "$ETA_ID"
echo ""

echo "--- CalculateSurge result ---"
wait_for_job "$SURGE_ID"
echo ""

echo "--- UpdateRideStatus (final) ---"
wait_for_job "$RIDE_ID"
echo ""

echo "=== Done ==="
