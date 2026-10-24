#!/usr/bin/env bash
# hermes-phone-access.sh — temporary LAN access to the Hermes web dashboard,
# for a phone.
#
#   hermes-phone-access.sh open  [--no-pair] [--idle 30m] [--max 4h] [--ip A.B.C.D]
#                                [--kdeconnect-device ID] [--allow-unauthenticated]
#                                [--dry-run]
#   hermes-phone-access.sh close
#   hermes-phone-access.sh status
#   hermes-phone-access.sh _tick        (internal: called by the systemd user watchdog)
#
# What it does
#  - the Hermes dashboard is never rebound; it stays on 127.0.0.1:9119, so
#    opening or closing phone access can never restart it or interrupt sessions,
#  - if the dashboard is not running, `open` starts it on localhost first,
#  - `open` starts a socat listener on this machine's LAN address that forwards
#    to 127.0.0.1:9119, so a phone on the same Wi-Fi reaches the web UI at
#    http://<ip>:9119,
#  - `open` sends the dashboard URL via KDE Connect (no QR shown); if sending
#    fails, it prints the URL instead. Pass --no-pair to skip pairing,
#  - `close` (or the idle watchdog) stops that listener; the LAN port is then
#    gone. Nothing is firewalled and nothing needs root,
#  - the listener runs as a transient systemd --user unit, so stopping it also
#    reaps its per-connection children.
#
# Requires: socat, systemd-run, iproute2 (ip, ss), curl, flock, systemd --user.
# Auth: phone access requires Hermes dashboard basic auth to be configured
# (HERMES_DASHBOARD_BASIC_AUTH_USERNAME + _PASSWORD, or _PASSWORD_HASH), unless
# --allow-unauthenticated is passed (not recommended).

set -euo pipefail

SELF="$(readlink -f "${BASH_SOURCE[0]}")"
PORT=9119
PROXY_UNIT="hermes-phone-proxy"
UNIT="hermes-phone-watchdog"
IDLE_DEFAULT="30m"
TICK=60

STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/hermes-phone"
STATE="$STATE_DIR/state"
LOG="$STATE_DIR/watchdog.log"
LOCK="$STATE_DIR/lock"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"

# --- options (filled by parse_args) -----------------------------------------
CMD=""
OPT_PAIR=1
OPT_DRY=0
OPT_IDLE="$IDLE_DEFAULT"
OPT_MAX="0"
OPT_IP=""
OPT_KDE_DEVICE=""
OPT_ALLOW_UNAUTH=0
POSITIONALS=()

# --- small helpers ----------------------------------------------------------

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
note() { printf '%s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }

need() { command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"; }

parse_duration() {
  local v="$1"
  [[ "$v" == "0" ]] && { printf '0'; return; }
  if [[ "$v" =~ ^([0-9]+)([smhd]?)$ ]]; then
    local n="${BASH_REMATCH[1]}" u="${BASH_REMATCH[2]}"
    case "$u" in
      ""|"s") printf '%s' "$n" ;;
      m) printf '%s' "$((n * 60))" ;;
      h) printf '%s' "$((n * 3600))" ;;
      d) printf '%s' "$((n * 86400))" ;;
    esac
  else
    die "bad duration: $v (use 30m, 4h, 90s or 0)"
  fi
}

# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() { command -v kdeconnect-cli >/dev/null 2>&1; }

find_kdeconnect_device() {
  local line id
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    printf '%s' "$id"
    return 0
  done < <(kdeconnect-cli -l 2>/dev/null || true)
  return 1
}

send_link_via_kdeconnect() {
  local url="$1"
  kdeconnect_available || return 1
  local dev
  dev="$(find_kdeconnect_device)" || return 1
  kdeconnect-cli -d "$dev" --share-text "$url" >/dev/null 2>&1
}

fmt_duration() {
  local s="$1"
  if (( s <= 0 )); then printf 'none'; return; fi
  if (( s >= 3600 )); then printf '%dh%02dm' $((s / 3600)) $(((s % 3600) / 60))
  elif (( s >= 60 )); then printf '%dm' $((s / 60))
  else printf '%ds' "$s"; fi
}

now_epoch() { date +%s; }

# --- state file -------------------------------------------------------------

state_init() { mkdir -p "$STATE_DIR"; [[ -f "$STATE" ]] || : >"$STATE"; }

state_get() {
  local key="$1"
  [[ -f "$STATE" ]] || return 0
  sed -n "s/^${key}=//p" "$STATE" | tail -1
}

state_set() {
  local key="$1"; shift
  [[ "$OPT_DRY" == "1" ]] && return 0
  local tmp="$STATE.tmp.$$"
  state_init
  { grep -v "^${key}=" "$STATE" 2>/dev/null || true; printf '%s=%s\n' "$key" "$*"; } >"$tmp"
  mv "$tmp" "$STATE"
}

state_clear() { [[ "$OPT_DRY" == "1" ]] && return 0; : >"$STATE"; }

log_line() {
  [[ "$OPT_DRY" == "1" ]] && return 0
  state_init
  printf '%s %s\n' "$(date -Is)" "$*" >>"$LOG"
  tail -n 500 "$LOG" >"$LOG.cut" 2>/dev/null && mv "$LOG.cut" "$LOG" || true
}

# --- helpers: LAN address, dashboard reachability ---------------------------

detect_ip() {
  local ip=""
  if [[ -n "$OPT_IP" ]]; then
    ip="$OPT_IP"
  else
    ip="$(ip -4 route get 1.1.1.1 2>/dev/null \
          | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')"
    if [[ -z "$ip" ]]; then
      ip="$(ip -4 -o addr show scope global 2>/dev/null \
            | awk '{split($4,a,"/"); print a[1]}' \
            | grep -vE '^(127\.|172\.1[6-9]\.)' | head -1)"
    fi
  fi
  [[ -n "$ip" ]] || die "cannot determine LAN address (pass --ip)"
  [[ "$ip" == *.*.*.* ]] || die "not an IPv4 LAN address: $ip"
  printf '%s' "$ip"
}

ip_prefix() { printf '%s' "${1%.*}."; }

probe() {
  curl -s -o /dev/null --max-time 4 "http://$1:$PORT/" 2>/dev/null
}

listening_on() {
  ss -tlnH "sport = :$PORT" 2>/dev/null | awk '{print $4}' | grep -qx "$1:$PORT"
}

dashboard_accepts_lan() {
  local code
  code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 -H "Host: $1:$PORT" "http://127.0.0.1:$PORT/" 2>/dev/null)"
  # 400 means the Host guard rejected it; anything else (200/302/401) means
  # the LAN host is accepted (302 = redirect to /login, i.e. auth gate active).
  [[ "$code" != "400" && "$code" != "000" ]]
}

ensure_dashboard() {
  if probe 127.0.0.1; then
    # Already running: make sure it trusts the LAN host, otherwise restart
    # with HERMES_DASHBOARD_PUBLIC_URL set so the phone's Host header is
    # accepted (Host/Origin guard, GHSA-ppp5-vxwm-4cf7) instead of a 400.
    if dashboard_accepts_lan "$ip"; then
      return 0
    fi
    note "Dashboard running but not trusting $ip - restarting with public_url..."
    if [[ "$OPT_DRY" == "1" ]]; then
      note "  [dry-run] would restart hermes dashboard with HERMES_DASHBOARD_PUBLIC_URL=http://$ip:$PORT"
      return 0
    fi
    # Kill the existing dashboard by the port it holds (pkill -f is unreliable
    # here because the process cmdline is the hermes python launcher).
    local oldpid
    oldpid="$(ss -tlnpH "sport = :$PORT" 2>/dev/null | sed -n 's/.*pid=\([0-9]*\).*/\1/p' | head -1)"
    if [[ -z "$oldpid" ]]; then
      oldpid="$(fuser "$PORT/tcp" 2>/dev/null | tr -s ' ' '\n' | head -1)"
    fi
    if [[ -n "$oldpid" ]]; then
      kill "$oldpid" 2>/dev/null || true
      for _ in $(seq 1 20); do kill -0 "$oldpid" 2>/dev/null || break; sleep 0.5; done
      kill -9 "$oldpid" 2>/dev/null || true
    fi
    for _ in $(seq 1 20); do probe 127.0.0.1 || break; sleep 0.5; done
  else
    if ! command -v hermes >/dev/null 2>&1; then
      die "Hermes dashboard is not running on 127.0.0.1:$PORT and 'hermes' is not on PATH. Start it with: hermes dashboard --no-open --port $PORT"
    fi
    note "Hermes dashboard not running on 127.0.0.1:$PORT - starting it..."
  fi
  if [[ -f "$HOME/.hermes/.env" ]]; then
    set -a; . "$HOME/.hermes/.env"; set +a
  fi
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] HERMES_DASHBOARD_PUBLIC_URL=http://$ip:$PORT hermes dashboard --no-open --port $PORT"
    return 0
  fi
  HERMES_DASHBOARD_PUBLIC_URL="http://$ip:$PORT" nohup hermes dashboard --no-open --port "$PORT" >/tmp/hermes-dashboard-phone.log 2>&1 &
  local pid=$!
  for _ in $(seq 1 40); do
    probe 127.0.0.1 && dashboard_accepts_lan "$ip" && { note "started (pid $pid)"; return 0; }
    sleep 0.5
  done
  die "could not start Hermes dashboard on 127.0.0.1:$PORT (see /tmp/hermes-dashboard-phone.log)"
}

# --- auth helpers -----------------------------------------------------------

auth_configured() {
  if [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_USERNAME:-}" ]] && { [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_PASSWORD:-}" ]] || [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_PASSWORD_HASH:-}" ]]; }; then
    return 0
  fi
  if [[ -f "$HOME/.hermes/config.yaml" ]] && grep -Eq '^[[:space:]]*username:[[:space:]]*[^[:space:]#]+' "$HOME/.hermes/config.yaml" 2>/dev/null; then
    return 0
  fi
  return 1
}

get_username() {
  if [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_USERNAME:-}" ]]; then
    printf '%s' "$HERMES_DASHBOARD_BASIC_AUTH_USERNAME"
  else
    printf 'admin'
  fi
}

get_password_display() {
  if [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_PASSWORD:-}" ]]; then
    printf '%s' "$HERMES_DASHBOARD_BASIC_AUTH_PASSWORD"
  elif [[ -n "${HERMES_DASHBOARD_BASIC_AUTH_PASSWORD_HASH:-}" ]]; then
    printf '(configured via password_hash - use your configured password)'
  else
    printf '(see your basic_auth config)'
  fi
}

# --- dry-run aware command execution ---------------------------------------

run() {
  if [[ "$OPT_DRY" == "1" ]]; then printf '  [dry-run] %s\n' "$*"; return 0; fi
  "$@"
}

# --- systemd user units (watchdog) ------------------------------------------

install_units() {
  [[ "$OPT_DRY" == "1" ]] && { note "  [dry-run] install $UNIT.{service,timer}"; return 0; }
  mkdir -p "$UNIT_DIR"
  cat >"$UNIT_DIR/$UNIT.service" <<EOF
[Unit]
Description=Hermes phone-access watchdog (idle auto-close)

[Service]
Type=oneshot
Environment=PATH=$HOME/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ExecStart=$SELF _tick
EOF
  cat >"$UNIT_DIR/$UNIT.timer" <<EOF
[Unit]
Description=Hermes phone-access watchdog timer

[Timer]
OnBootSec=2min
OnActiveSec=10s
OnUnitActiveSec=${TICK}s
AccuracySec=5s

[Install]
WantedBy=timers.target
EOF
  systemctl --user daemon-reload
  systemctl --user enable --now "$UNIT.timer" >/dev/null 2>&1 || warn "could not enable $UNIT.timer"
}

watchdog_state() { systemctl --user is-active "$UNIT.timer" 2>/dev/null || true; }

# --- the LAN listener (socat, transient user unit) --------------------------

proxy_state() { systemctl --user is-active "$PROXY_UNIT" 2>/dev/null || true; }

proxy_start() {
  local ip="$1"
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] systemd-run --user --unit=$PROXY_UNIT socat TCP-LISTEN:$PORT,bind=$ip,reuseaddr,fork TCP:127.0.0.1:$PORT"
    return 0
  fi
  systemctl --user stop "$PROXY_UNIT" >/dev/null 2>&1 || true
  systemctl --user reset-failed "$PROXY_UNIT" >/dev/null 2>&1 || true
  systemd-run --user --unit="$PROXY_UNIT" --collect \
    --description="Hermes phone proxy on $ip:$PORT" \
    socat "TCP-LISTEN:$PORT,bind=$ip,reuseaddr,fork" "TCP:127.0.0.1:$PORT" >/dev/null
}

proxy_stop() { run systemctl --user stop "$PROXY_UNIT" 2>/dev/null || true; }

# --- activity measurement ---------------------------------------------------

lan_traffic() {
  local prefix="$1"
  ss -tniH state established "( sport = :$PORT or dport = :$PORT )" 2>/dev/null | awk -v pfx="$prefix" '
    /bytes_/ {
      if (ok) {
        for (i = 1; i <= NF; i++) {
          if ($i ~ /^bytes_sent:/)     { split($i, a, ":"); sent += a[2] }
          if ($i ~ /^bytes_received:/) { split($i, b, ":"); recv += b[2] }
        }
      }
      next
    }
    {
      peer = $NF
      sub(/:[0-9]+$/, "", peer)
      gsub(/^\[|\]$/, "", peer)
      ok = (index(peer, pfx) > 0) && (peer !~ /^127\./)
      if (ok) peers[peer] = 1
    }
    END {
      list = ""
      for (p in peers) list = (list == "" ? p : list "," p)
      printf "%d %d %s", sent + 0, recv + 0, list
    }'
}

clipboard_tool() {
  if [[ -n "${WAYLAND_DISPLAY:-}" ]] && command -v wl-copy >/dev/null 2>&1; then
    printf 'wl-copy'
  elif command -v xclip >/dev/null 2>&1; then
    printf 'xclip -selection clipboard'
  elif command -v xsel >/dev/null 2>&1; then
    printf 'xsel -b'
  fi
}

print_password_hint() {
  local pw="$1"
  [[ -n "$pw" ]] || return 0
  local clip; clip="$(clipboard_tool)"
  note ""
  note "  Easier than typing that password - push it to the phone:"
  if [[ -z "$clip" ]]; then
    note "     (no clipboard tool found: install wl-clipboard, xclip or xsel)"
  else
    note "     printf '%s' '$pw' | $clip"
  fi
  if kdeconnect_available; then
    local line id name found=0
    while IFS= read -r line; do
      id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
      [[ -n "$id" ]] || continue
      name="$(printf '%s' "$line" | sed -nE 's/^- *(.*): [a-f0-9]{32}.*/\1/p')"
      [[ "$line" == *reachable* ]] || name="$name (not reachable)"
      note "     kdeconnect-cli -d $id --send-clipboard    # $name"
      found=1
    done < <(kdeconnect-cli -l 2>/dev/null || true)
    [[ "$found" == "1" ]] || note "     kdeconnect-cli -d <device-id> --send-clipboard   # kdeconnect-cli -l"
  fi
}

print_connect_help() {
  local ip="$1" url="http://$ip:$PORT"
  note ""
  note "Connect the phone (same Wi-Fi):"

  if [[ "$OPT_PAIR" == "1" ]]; then
    if send_link_via_kdeconnect "$url"; then
      note ""
      note "  1) Sent dashboard URL to phone via KDE Connect"
      note "     (also pushed it to the phone clipboard as a fallback)"
      note "     If no notification appears, long-press the address bar -> Paste."
      # Best-effort: put the URL on the phone clipboard too, in case the
      # share-text notification is missed (some Android OEMs suppress it).
      _clip="$(clipboard_tool)"
      if [[ -n "$_clip" ]]; then
        printf '%s' "$url" | timeout 3 sh -c "$_clip" >/dev/null 2>&1 || true
      fi
      kdeconnect-cli -d "$(find_kdeconnect_device)" --send-clipboard >/dev/null 2>&1 || true
    else
      note ""
      note "  1) Open in the phone browser:"
    fi
  else
    note ""
    note "  1) Open in the phone browser:"
  fi

  note "     $url"
  note ""
  local u p actual_pw
  u="$(get_username)"; p="$(get_password_display)"
  actual_pw="${HERMES_DASHBOARD_BASIC_AUTH_PASSWORD:-}"
  note "  2) Sign in:"
  note "     username: $u"
  note "     password: $p"
  print_password_hint "$actual_pw"
  note ""
  note "     then use 'Add to Home Screen' for an app-like icon."
  if [[ "$OPT_PAIR" != "1" ]]; then
    note ""
    note "  (drop --no-pair to send the URL via KDE Connect)"
  fi
}

# --- open -------------------------------------------------------------------

cmd_open() {
  # Load dashboard env (basic auth creds etc.) if present.
  if [[ -f "$HOME/.hermes/.env" ]]; then
    set -a; . "$HOME/.hermes/.env"; set +a
  fi
  need socat; need systemd-run; need ip; need ss; need curl; need flock
  local ip prefix idle_s max_s
  ip="${OPT_IP:-$(detect_ip)}"
  prefix="$(ip_prefix "$ip")"
  idle_s="$(parse_duration "$OPT_IDLE")"
  max_s="$(parse_duration "$OPT_MAX")"

  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || die "another hermes-phone-access command is running"

  local status prev_ip
  status="$(state_get STATUS)"
  prev_ip="$(state_get IP)"

  if [[ "$status" == "open" && "$prev_ip" == "$ip" ]]; then
    note "phone access already open on $ip:$PORT - refreshing timeouts"
    state_set IDLE "$idle_s"
    state_set MAX "$max_s"
    state_set LAST_ACTIVITY "$(now_epoch)"
    state_set PEERS ""
    install_units
    print_connect_help "$ip"
    exec 9>&-          # release the lock: the watchdog must be able to tick
    if [[ "$OPT_DRY" == "1" ]]; then note "(dry-run: not waiting for the watchdog)"; return; fi
    wait_until_closed
    return
  fi
  if [[ "$status" == "open" ]]; then
    note "phone access was open on $prev_ip - moving to $ip"
    do_close "reopen" || true
  fi

  note "== hermes dashboard phone access =="
  note "phone URL     : http://$ip:$PORT"

  # Auth is mandatory for LAN exposure: a non-loopback public_url engages the
  # auth gate, and the dashboard refuses to start without a configured provider.
  if ! auth_configured; then
    die "Hermes dashboard basic auth is not configured, which is required for phone access. Set in ~/.hermes/.env:\n  HERMES_DASHBOARD_BASIC_AUTH_USERNAME=admin\n  HERMES_DASHBOARD_BASIC_AUTH_PASSWORD=<strong password>\n  HERMES_DASHBOARD_BASIC_AUTH_SECRET=\$(openssl rand -base64 32)\nThen re-run. (There is no unauthenticated LAN mode.)"
  fi

  ensure_dashboard

  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] skipping the server reachability probe on 127.0.0.1:$PORT"
  elif ! probe 127.0.0.1; then
    die "the Hermes dashboard is not answering on 127.0.0.1:$PORT"
  fi
  if listening_on "$ip" && ! [[ "$OPT_DRY" == "1" ]]; then
    die "$ip:$PORT is already taken"
  fi

  proxy_start "$ip"
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  (dry-run: skipping the reachability probe)"
  elif ! probe "$ip"; then
    proxy_stop
    die "the proxy did not come up on $ip:$PORT"
  fi
  note "listener      : $ip:$PORT -> 127.0.0.1:$PORT ($(proxy_state))"

  state_clear
  state_set STATUS open
  state_set OPENED_AT "$(now_epoch)"
  state_set LAST_ACTIVITY "$(now_epoch)"
  state_set IDLE "$idle_s"
  state_set MAX "$max_s"
  state_set IP "$ip"
  state_set PREFIX "$prefix"
  state_set PEERS ""
  state_set BYTES 0
  install_units
  note "watchdog      : idle timeout $(fmt_duration "$idle_s")$(
    [[ "$max_s" != "0" ]] && printf ', hard stop after %s' "$(fmt_duration "$max_s")"
  ) (timer $(watchdog_state))"
  log_line "OPEN ip=$ip idle=${idle_s}s max=${max_s}s"

  print_connect_help "$ip"
  note ""
  note "Ctrl+C now, or just stop using the phone - either way access closes."
  exec 9>&-            # release the lock: the watchdog must be able to tick
  if [[ "$OPT_DRY" == "1" ]]; then note "(dry-run: not waiting for the watchdog)"; return; fi
  wait_until_closed
}

wait_until_closed() {
  local cleared=0
  trap 'cleared=1; do_close "interrupt" || true; exit 0' INT TERM
  while [[ "$(state_get STATUS)" == "open" ]]; do
    sleep 5
  done
  [[ "$cleared" == "0" ]] || return 0
  print_close_summary
}

# --- close ------------------------------------------------------------------

print_close_summary() {
  note "phone access closed"
  note "listener     : stopped (dashboard stays on 127.0.0.1:$PORT)"
  note "watchdog     : timer $(watchdog_state) (self-heals after reboot)"
  note ""
  note "The dashboard keeps running here; run 'hermes-phone-access.sh open' again to work from the phone."
}

do_close() {
  local reason="$1" prev_ip
  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || { warn "close: lock busy"; return 1; }

  prev_ip="$(state_get IP)"
  proxy_stop

  if [[ "$(state_get STATUS)" != "open" && "$(proxy_state)" != "active" && "$OPT_DRY" != "1" ]]; then
    log_line "CLOSE no-op (already closed)"
    return 0
  fi

  state_set STATUS closed
  state_set PEERS ""
  state_set BYTES 0
  state_set LAST_ACTIVITY "$(now_epoch)"
  log_line "CLOSE reason=$reason ip=$prev_ip"
  return 0
}

cmd_close() {
  need flock
  do_close "manual"
  print_close_summary
}

# --- watchdog tick ----------------------------------------------------------

cmd_tick() {
  need flock
  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || exit 0

  local status
  status="$(state_get STATUS)"

  if [[ "$status" != "open" ]]; then
    if [[ "$(proxy_state)" == "active" ]]; then
      proxy_stop
      log_line "HEAL stopped stray listener"
    fi
    exit 0
  fi

  if [[ "$(proxy_state)" != "active" ]]; then
    log_line "AUTO-CLOSE listener gone"
    do_close "listener-gone" || true
    exit 0
  fi

  local ip prefix idle last peers_now last_peers bytes_now bytes_last t
  ip="$(state_get IP)"; prefix="$(state_get PREFIX)"
  idle="$(state_get IDLE)"; last="$(state_get LAST_ACTIVITY)"
  last_peers="$(state_get PEERS)"; bytes_last="$(state_get BYTES)"
  [[ -n "$prefix" ]] || prefix="$(ip_prefix "$ip")"

  t="$(lan_traffic "$prefix")"
  bytes_now="${t%% *}"; t="${t#* }"
  local recv_now="${t%% *}"; peers_now="${t#* }"
  local total_now=$((bytes_now + recv_now))

  if [[ "$peers_now" != "$last_peers" ]] || (( total_now > ${bytes_last:-0} )); then
    state_set LAST_ACTIVITY "$(now_epoch)"
    state_set PEERS "$peers_now"
    state_set BYTES "$total_now"
  elif (( total_now < ${bytes_last:-0} )); then
    state_set BYTES "$total_now"
    state_set PEERS "$peers_now"
  fi

  local now last_act max opened
  now="$(now_epoch)"
  last_act="$(state_get LAST_ACTIVITY)"
  max="$(state_get MAX)"; opened="$(state_get OPENED_AT)"
  [[ -n "$last_act" ]] || last_act="$now"

  if (( now - last_act > idle )); then
    log_line "AUTO-CLOSE idle for $((now - last_act))s (limit ${idle}s)"
    do_close "idle" || true
    exit 0
  fi
  if [[ -n "$max" && "$max" != "0" ]] && (( now - ${opened:-$now} > max )); then
    log_line "AUTO-CLOSE hard stop after $((now - ${opened:-$now}))s (limit ${max}s)"
    do_close "max-lifetime" || true
    exit 0
  fi
  exit 0
}

# --- status -----------------------------------------------------------------

cmd_status() {
  need flock
  if [[ -f "$HOME/.hermes/.env" ]]; then
    set -a; . "$HOME/.hermes/.env"; set +a
  fi
  local status ip idle last_act peers opened max now idle_for
  status="$(state_get STATUS)"
  ip="$(state_get IP)"

  if [[ "$status" != "open" ]]; then
    note "phone access : CLOSED (listener $(proxy_state))"
    if [[ -s "$STATE" ]]; then
      note "last change  : $(date -d "@$(state_get LAST_ACTIVITY)" '+%H:%M' 2>/dev/null || echo '?')"
    fi
    note "watchdog     : timer $(watchdog_state)"
    return 0
  fi

  idle="$(state_get IDLE)"; last_act="$(state_get LAST_ACTIVITY)"
  peers="$(state_get PEERS)"; opened="$(state_get OPENED_AT)"; max="$(state_get MAX)"
  now="$(now_epoch)"; idle_for=$(( now - last_act ))

  note "phone access : OPEN since $(date -d "@$opened" '+%H:%M' 2>/dev/null || echo "$opened")"
  note "phone URL    : http://$ip:$PORT   (listener $(proxy_state))"
  note "phone peers  : ${peers:-none}   ($(state_get BYTES) bytes seen)"
  note "last activity: $(( now - last_act ))s ago"
  note "auto-close   : in $(fmt_duration $(( idle - idle_for ))) of idle$(
    [[ -n "$max" && "$max" != "0" ]] && printf ' (hard stop in %s)' "$(fmt_duration $(( max - (now - opened) )))"
  )"
  note "auth         : $(auth_configured && echo 'configured' || echo 'NOT configured')"
  note "watchdog     : timer $(watchdog_state)"
  note "log          : $LOG"
}

# --- wiring -----------------------------------------------------------------

usage() {
  awk 'NR == 1 { next } /^[^#]/ { exit } { sub(/^# ?/, ""); print }' "$SELF"
  exit "${1:-2}"
}

parse_args() {
  [[ $# -gt 0 ]] || usage
  CMD="$1"; shift
  case "$CMD" in
    open|close|status|_tick) ;;
    -h|--help|help) usage 0 ;;
    *) die "unknown command: $CMD" ;;
  esac
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --pair) OPT_PAIR=1 ;;
      --no-pair) OPT_PAIR=0 ;;
      --dry-run) OPT_DRY=1 ;;
      --idle) OPT_IDLE="${2:?--idle needs a value}"; shift ;;
      --max) OPT_MAX="${2:?--max needs a value}"; shift ;;
      --ip) OPT_IP="${2:?--ip needs a value}"; shift ;;
      --kdeconnect-device) OPT_KDE_DEVICE="${2:?--kdeconnect-device needs a value}"; shift ;;
      --allow-unauthenticated) OPT_ALLOW_UNAUTH=1 ;;
      -h|--help) usage 0 ;;
      --*) die "unknown option: $1" ;;
      *) POSITIONALS+=("$1") ;;
    esac
    shift
  done
}

main() {
  parse_args "$@"
  case "$CMD" in
    open) cmd_open ;;
    close) cmd_close ;;
    status) cmd_status ;;
    _tick) cmd_tick ;;
  esac
}

main "$@"
