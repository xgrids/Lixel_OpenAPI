#!/bin/bash
# Starts a throw-away Mosquitto broker, runs a command against it, stops the broker.
#
#   run_with_broker.sh <command> [args...]
#
# The command gets these environment variables:
#   LIXEL_TEST_PLAIN_PORT   plain MQTT, anonymous              (like a K1 / L2PRO device)
#   LIXEL_TEST_TLS_PORT     MQTTS with a user name and password (like a K2 / H1 / F1 / Porcam device)
#   LIXEL_TEST_CA_FILE      the broker's self-signed certificate, to be used as the trusted CA
#   LIXEL_TEST_USER / LIXEL_TEST_PASSWORD   an account of the TLS listener
# Ports are picked free at run time. The certificate and the password file are generated into a
# temporary directory that is removed at the end, so no key is ever stored in the repository.
# Needs: mosquitto, mosquitto_passwd, openssl.
set -u

if [ $# -lt 1 ]; then echo "usage: $0 <command> [args...]" >&2; exit 2; fi
for tool in mosquitto mosquitto_passwd openssl; do
  command -v "$tool" > /dev/null || { echo "$tool not found" >&2; exit 2; }
done

DIR="$(mktemp -d /tmp/lixel-broker.XXXXXX)"
BROKER_PID=""
cleanup() {
  [ -n "$BROKER_PID" ] && kill "$BROKER_PID" 2> /dev/null && wait "$BROKER_PID" 2> /dev/null
  rm -rf "$DIR"
}
trap cleanup EXIT INT TERM

free_port() {
  python3 - << 'PY'
import socket
s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()
PY
}
PLAIN="$(free_port)"
TLS="$(free_port)"
USER_NAME="testuser"
PASSWORD="testpass"   # test-only value; the broker only listens on 127.0.0.1

openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" \
  -keyout "$DIR/server.key" -out "$DIR/server.crt" > /dev/null 2>&1 || { echo "openssl failed" >&2; exit 2; }
mosquitto_passwd -b -c "$DIR/passwd" "$USER_NAME" "$PASSWORD" > /dev/null 2>&1 || { echo "mosquitto_passwd failed" >&2; exit 2; }

cat > "$DIR/broker.conf" << EOF
per_listener_settings true
persistence false
log_dest stderr
log_type error
log_type warning

listener $PLAIN 127.0.0.1
protocol mqtt
allow_anonymous true

listener $TLS 127.0.0.1
protocol mqtt
cafile $DIR/server.crt
certfile $DIR/server.crt
keyfile $DIR/server.key
tls_version tlsv1.2
allow_anonymous false
password_file $DIR/passwd
EOF

mosquitto -c "$DIR/broker.conf" > "$DIR/broker.log" 2>&1 &
BROKER_PID=$!

# wait until both listeners accept connections
for _ in $(seq 1 50); do
  if (exec 3<> "/dev/tcp/127.0.0.1/$PLAIN") 2> /dev/null && (exec 3<> "/dev/tcp/127.0.0.1/$TLS") 2> /dev/null; then READY=1; break; fi
  kill -0 "$BROKER_PID" 2> /dev/null || { echo "broker died:" >&2; cat "$DIR/broker.log" >&2; exit 2; }
  sleep 0.1
done
[ "${READY:-0}" = "1" ] || { echo "broker did not start" >&2; cat "$DIR/broker.log" >&2; exit 2; }

export LIXEL_TEST_PLAIN_PORT="$PLAIN" LIXEL_TEST_TLS_PORT="$TLS" LIXEL_TEST_CA_FILE="$DIR/server.crt"
export LIXEL_TEST_USER="$USER_NAME" LIXEL_TEST_PASSWORD="$PASSWORD"
"$@"
RC=$?
exit $RC
