#!/usr/bin/env bash
#          Tokenize
#         /        \
#    Lowercase   CountWords
#         \        /
#       FormatReport
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080}"

ORIGINAL_TEXT='Re-file ticket INC-901; both SRE pods bounced yet latency never cleared.'

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

echo "Original text: $ORIGINAL_TEXT"
echo ""

_json_text=${ORIGINAL_TEXT//\\/\\\\}
_json_text=${_json_text//\"/\\\"}

echo "=== 1. Submitting Tokenize ==="
RESP=$(post_job '{"kind":"run","name":"tokenize","body":{"args":{"text":"'"$_json_text"'"}}}')
TOKENIZE_ID=$(extract_id "$RESP")
echo "    Tokenize id: $TOKENIZE_ID"

echo "=== 2. Submitting Lowercase ==="
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"lowercase\",
  \"body\": {
    \"args\": {},
    \"dependsOn\": [\"$TOKENIZE_ID\"]
  }
}")
LOWERCASE_ID=$(extract_id "$RESP")
echo "    Lowercase id: $LOWERCASE_ID"

echo "=== 3. Submitting CountWords ==="
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"count_words\",
  \"body\": {
    \"args\": {},
    \"dependsOn\": [\"$TOKENIZE_ID\"]
  }
}")
COUNT_ID=$(extract_id "$RESP")
echo "    CountWords id: $COUNT_ID"

echo "=== 4. Submitting FormatReport ==="
RESP=$(post_job "{
  \"kind\": \"run\",
  \"name\": \"format_report\",
  \"body\": {
    \"args\": {},
    \"dependsOn\": [\"$LOWERCASE_ID\", \"$COUNT_ID\"]
  }
}")
REPORT_ID=$(extract_id "$RESP")
echo "    FormatReport id: $REPORT_ID"

echo ""
echo "=== Waiting for pipeline to complete... ==="
echo ""

echo "--- Tokenize result ---"
wait_for_job "$TOKENIZE_ID"
echo ""

echo "--- Lowercase result ---"
wait_for_job "$LOWERCASE_ID"
echo ""

echo "--- CountWords result ---"
wait_for_job "$COUNT_ID"
echo ""

echo "--- FormatReport ---"
wait_for_job "$REPORT_ID"
echo ""

echo "=== Done ==="
