#!/bin/sh
# Simple smoke tests for the HTTP server.
# Usage: ./tests/test_requests.sh [base_url]
# Make sure the server is already running (e.g. ./build/http_server) before running this.

set -e
BASE_URL="${1:-http://127.0.0.1:8080}"

echo "== GET / =="
curl -is "$BASE_URL/" | head -n 5
echo

echo "== GET /does-not-exist (expect 404) =="
curl -is "$BASE_URL/does-not-exist" | head -n 1
echo

echo "== POST /upload =="
echo "hello world" > /tmp/http_server_test.txt
curl -is -X POST --data-binary @/tmp/http_server_test.txt \
    -H "X-Filename: http_server_test.txt" \
    "$BASE_URL/upload"
echo

echo "== GET /files/http_server_test.txt =="
curl -is "$BASE_URL/files/http_server_test.txt"
echo

echo "== DELETE /files/http_server_test.txt (expect 204) =="
curl -is -X DELETE "$BASE_URL/files/http_server_test.txt" | head -n 1
echo

echo "== PUT / (expect 405) =="
curl -is -X PUT "$BASE_URL/" | head -n 1
echo

echo "== GET /cgi-bin/hello.sh =="
curl -is "$BASE_URL/cgi-bin/hello.sh"
echo

echo "All smoke tests sent."
