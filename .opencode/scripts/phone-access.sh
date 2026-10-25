#!/usr/bin/env bash
# phone-access.sh — temporary LAN access to the OpenCode server, for a phone.
#
#   phone-access.sh open  [--no-pair] [--idle 30m] [--max 4h] [--ip A.B.C.D] [--kdeconnect-device ID]
#                         [--dry-run]
#   phone-access.sh close
#   phone-access.sh status
#   phone-access.sh reset-bind   (one-time: return the server to localhost only)
#   phone-access.sh _tick        (internal: called by the systemd user watchdog)
#
# What it does
#  - the OpenCode server is never rebound, so opening or closing phone access
#    can never restart the server or interrupt your sessions,
#  - `open` starts a socat listener on this machine's LAN address that forwards
#    to 127.0.0.1:49374, so a phone on the same Wi-Fi reaches the web UI at
#    http://<ip>:49374,
#  - `open` sends the one-time sign-in link via KDE Connect (no QR shown);
#    if sending fails, it prints the link instead. Pass --no-pair to skip pairing,
#  - `close` (or the idle watchdog) stops that listener; the LAN port is then
#    gone. Nothing is firewalled and nothing needs root.
#  - the listener runs as a transient systemd --user unit, so stopping it also
#    reaps its per-connection children.
#
# Requires: socat, systemd-run, iproute2 (ip, ss), curl, flock, systemd --user.
set -euo pipefail

SELF="$(readlink -f "${BASH_SOURCE[0]}")"
PORT=49374
PROXY_UNIT="opencode-phone-proxy"
UNIT="opencode-phone-watchdog"
IDLE_DEFAULT="30m"
TICK=60

STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/opencode-phone"
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
POSITIONALS=()

# --- small helpers ----------------------------------------------------------

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
note() { printf '%s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }

need() { command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"; }

# Parse "30m" / "90s" / "4h" / "0" into seconds.
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

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
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
#
# key=value, rewritten atomically. Written by `open`, updated by `_tick`:
#   STATUS OPENED_AT LAST_ACTIVITY IDLE MAX IP PREFIX PEERS BYTES

state_init() { mkdir -p "$STATE_DIR"; [[ -f "$STATE" ]] || : >"$STATE"; }

state_get() {
  local key="$1"
  [[ -f "$STATE" ]] || return 0
  sed -n "s/^${key}=//p" "$STATE" | tail -1
}

# state_set KEY VALUE... (atomic: rewrite through a temp file)
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

# --- helpers: LAN address, opencode bind, the proxy -------------------------

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

# Address the OpenCode service is bound to ("" means its default, localhost).
svc_hostname() { opencode service get hostname 2>/dev/null || true; }

# The proxy only makes sense while the server stays on loopback.
bind_is_private() {
  local h; h="$(svc_hostname)"
  [[ -z "$h" || "$h" == "127.0.0.1" || "$h" == "localhost" ]]
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
Description=OpenCode phone-access watchdog (idle auto-close)

[Service]
Type=oneshot
Environment=PATH=$HOME/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ExecStart=$SELF _tick
EOF
  cat >"$UNIT_DIR/$UNIT.timer" <<EOF
[Unit]
Description=OpenCode phone-access watchdog timer

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

watchdog_state() {
  systemctl --user is-active "$UNIT.timer" 2>/dev/null || true
}

# --- the LAN listener (socat, transient user unit) --------------------------

proxy_state() {
  systemctl --user is-active "$PROXY_UNIT" 2>/dev/null || true
}

proxy_start() {
  local ip="$1"
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] systemd-run --user --unit=$PROXY_UNIT socat TCP-LISTEN:$PORT,bind=$ip,reuseaddr,fork TCP:127.0.0.1:$PORT"
    return 0
  fi
  systemctl --user stop "$PROXY_UNIT" >/dev/null 2>&1 || true
  systemctl --user reset-failed "$PROXY_UNIT" >/dev/null 2>&1 || true
  systemd-run --user --unit="$PROXY_UNIT" --collect \
    --description="OpenCode phone proxy on $ip:$PORT" \
    socat "TCP-LISTEN:$PORT,bind=$ip,reuseaddr,fork" "TCP:127.0.0.1:$PORT" >/dev/null
}

proxy_stop() {
  run systemctl --user stop "$PROXY_UNIT" 2>/dev/null || true
}

# Is anything listening on the given address/port already?
listening_on() {
  ss -tlnH "sport = :$PORT" 2>/dev/null | awk '{print $4}' | grep -qx "$1:$PORT"
}

# Is the server answering? (any HTTP status counts)
probe() {
  curl -s -o /dev/null --max-time 4 "http://$1:$PORT/api/info"
}

# --- activity measurement ---------------------------------------------------
#
# Sum bytes on established connections to our port whose *peer* is inside the
# LAN subnet, and list those peers. Local clients (TUI, `opencode api`) connect
# over loopback and are ignored, so the machine's own traffic never counts.
# Both directions count as activity: bytes_received shows the phone sending
# prompts, bytes_sent shows the server still streaming to a watching phone.
#
# Prints: "<sent> <recv> <comma-separated peers>"
lan_traffic() {
  local prefix="$1"
  ss -tniH state established "( sport = :$PORT or dport = :$PORT )" 2>/dev/null | awk -v pfx="$prefix" '
    # Detail line: carries the byte counters for the socket above it.
    /bytes_/ {
      if (ok) {
        for (i = 1; i <= NF; i++) {
          if ($i ~ /^bytes_sent:/)     { split($i, a, ":"); sent += a[2] }
          if ($i ~ /^bytes_received:/) { split($i, b, ":"); recv += b[2] }
        }
      }
      next
    }
    # Socket line: <state?> <recv-q> <send-q> <local> <peer>. The state column
    # is omitted under some filters, so the peer is always the last field.
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

# --- open -------------------------------------------------------------------

print_connect_help() {
  local ip="$1" url="http://$ip:$PORT"
  note ""
  note "Connect the phone (same Wi-Fi):"

  if [[ "$OPT_PAIR" == "1" ]]; then
    local pair_out link
    pair_out=$(opencode pair --url "$url" 2>&1 || true)
    link=$(printf '%s\n' "$pair_out" | grep -Eo 'https?://[^[:space:]]+' | head -1)
    if send_link_via_kdeconnect "${link:-$url}"; then
      note ""
      note "  1) Sent one-time sign-in link to phone via KDE Connect"
    else
      note ""
      note "  1) One-time sign-in link (KDE Connect unavailable):"
      if [[ -n "$link" ]]; then
        note "     $link"
      else
        note "     $url"
      fi
    fi
    note "     (valid 5 min, single use; never forward the link to a chat app)"
    note ""
    note "  2) or sign in with the password:"
  else
    note ""
    note "  1) open in the phone browser and sign in:"
  fi

  note "     $url"
  note "     username: opencode"
  note "     password: \$(opencode service get password)"
  print_password_hint
  note ""
  note "     then use 'Add to Home Screen' for an app-like icon."
  if [[ "$OPT_PAIR" != "1" ]]; then
    note ""
    note "  (drop --no-pair to get a one-time sign-in link via KDE Connect)"
  fi
}


# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
}
# --- getting the password onto the phone without typing it ------------------

# The clipboard tool that works in this session, or empty.
clipboard_tool() {
  if [[ -n "${WAYLAND_DISPLAY:-}" ]] && command -v wl-copy >/dev/null 2>&1; then
    printf 'wl-copy'
  elif command -v xclip >/dev/null 2>&1; then
    printf 'xclip -selection clipboard'
  elif command -v xsel >/dev/null 2>&1; then
    printf 'xsel -b'
  fi
}


# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
}
print_password_hint() {
  local clip; clip="$(clipboard_tool)"
  note ""
  note "  Easier than typing that password — push it to the phone:"
  if [[ -z "$clip" ]]; then
    note "     (no clipboard tool found: install wl-clipboard, xclip or xsel)"
    return 0
  fi
  note "     opencode service get password | $clip"
  if ! command -v kdeconnect-cli >/dev/null 2>&1; then
    return 0
  fi
  local line id name found=0
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    name="$(printf '%s' "$line" | sed -nE 's/^- *(.*): [a-f0-9]{32}.*/\1/p')"
    [[ "$line" == *reachable* ]] || name="$name (not reachable)"
    note "     kdeconnect-cli -d $id --send-clipboard    # $name"
    found=1
  done < <(kdeconnect-cli -l 2>/dev/null || true)
  if [[ "$found" == "0" ]]; then
    note "     kdeconnect-cli -d <device-id> --send-clipboard   # kdeconnect-cli -l"
  fi
}


# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
}
cmd_open() {
  need socat; need systemd-run; need ip; need ss; need curl; need flock
  local ip prefix idle_s max_s
  ip="${OPT_IP:-$(detect_ip)}"
  prefix="$(ip_prefix "$ip")"
  idle_s="$(parse_duration "$OPT_IDLE")"
  max_s="$(parse_duration "$OPT_MAX")"

  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || die "another phone-access command is running"

  local status prev_ip
  status="$(state_get STATUS)"
  prev_ip="$(state_get IP)"

  if [[ "$status" == "open" && "$prev_ip" == "$ip" ]]; then
    note "phone access already open on $ip:$PORT — refreshing timeouts"
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
    note "phone access was open on $prev_ip — moving to $ip"
    do_close "reopen" || true
  fi

  note "== opencode phone access =="
  note "phone URL     : http://$ip:$PORT"

  # The barriere is the listener itself: the server stays on localhost and this
  # script never rebounds it, so your sessions are never interrupted.
  if ! bind_is_private; then
    warn "the OpenCode service is bound to $(svc_hostname), not localhost"
    warn "phone access will work, but the port stays reachable from the LAN even"
    warn "after 'close'. Run '$(basename "$SELF") reset-bind' once to fix that"
    warn "(it restarts the server one last time)."
  fi

  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] skipping the server reachability probe on 127.0.0.1:$PORT"
  elif ! probe 127.0.0.1; then
    if ! bind_is_private; then
      die "the server is bound to $(svc_hostname), so nothing listens on 127.0.0.1:$PORT — run '$(basename "$SELF") reset-bind' (one restart)"
    fi
    die "the OpenCode server is not answering on 127.0.0.1:$PORT — start it with 'opencode service start'"
  fi
  if listening_on "$ip" && ! [[ "$OPT_DRY" == "1" ]]; then
    die "$ip:$PORT is already taken (is the service bound to the LAN? see 'reset-bind')"
  fi

  # 1. start the LAN listener. This is the only moving part.
  # systemd-run returns before socat has actually bound the port, so wait
  # for the listener (and then for an HTTP answer) instead of failing on
  # the first connection-refused.
  proxy_start "$ip"
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  (dry-run: skipping the reachability probe)"
  else
    local proxy_ok=0
    for _ in $(seq 1 20); do
      if probe "$ip"; then
        proxy_ok=1
        break
      fi
      sleep 0.25
    done
    if [[ "$proxy_ok" != "1" ]]; then
      proxy_stop
      die "the proxy did not come up on $ip:$PORT"
    fi
  fi
  note "listener      : $ip:$PORT -> 127.0.0.1:$PORT ($(proxy_state))"

  # 2. state + watchdog.
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
  note "Ctrl+C now, or just stop using the phone — either way access closes."
  exec 9>&-            # release the lock: the watchdog must be able to tick
  if [[ "$OPT_DRY" == "1" ]]; then note "(dry-run: not waiting for the watchdog)"; return; fi
  wait_until_closed
}

# Stay in the foreground while the session is open, then report.
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
  note "listener     : stopped (server stays on 127.0.0.1:$PORT)"
  note "watchdog     : timer $(watchdog_state) (self-heals after reboot)"
  note ""
  note "The session itself keeps running here; press on with the TUI, or"
  note "run 'phone-access.sh open' again to work from the phone."
}

do_close() {
  local reason="$1" prev_ip
  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || { warn "close: lock busy"; return 1; }

  prev_ip="$(state_get IP)"
  proxy_stop

  # Nothing was open and no listener lingers? Then there is nothing to do.
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

  # Not open: the only job is to drop a listener left behind by a crash.
  if [[ "$status" != "open" ]]; then
    if [[ "$(proxy_state)" == "active" ]]; then
      proxy_stop
      log_line "HEAL stopped stray listener"
    fi
    exit 0
  fi

  # Open but the listener died: treat the session as closed.
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

  # Activity = byte movement in either direction, or a newly seen peer.
  if [[ "$peers_now" != "$last_peers" ]] || (( total_now > ${bytes_last:-0} )); then
    state_set LAST_ACTIVITY "$(now_epoch)"
    state_set PEERS "$peers_now"
    state_set BYTES "$total_now"
  elif (( total_now < ${bytes_last:-0} )); then
    # counters reset (connections churned): re-arm without calling it activity.
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
  local status ip idle last_act peers bytes opened max now idle_for
  status="$(state_get STATUS)"
  ip="$(state_get IP)"

  if [[ "$status" != "open" ]]; then
    note "phone access : CLOSED (listener $(proxy_state))"
    if [[ -s "$STATE" ]]; then
      note "last change  : $(date -d "@$(state_get LAST_ACTIVITY)" '+%H:%M' 2>/dev/null || echo '?')"
    fi
    bind_note
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
  bind_note
  note "watchdog     : timer $(watchdog_state)"
  note "log          : $LOG"
}

# Warn when the server itself is on the LAN: 'close' cannot hide it then.
bind_note() {
  if bind_is_private; then
    note "server bind  : private (127.0.0.1 only)"
  else
    warn "server bind  : $(svc_hostname) — reachable from the LAN even when closed"
    warn "               run 'phone-access.sh reset-bind' once to restore localhost"
  fi
}


# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
}
# --- one-time: put the server back on localhost -----------------------------

cmd_reset_bind() {
  need opencode
  local cur; cur="$(svc_hostname)"
  if bind_is_private; then
    note "the server is already private (${cur:-default}); nothing to do"
    return 0
  fi
  note "moving the OpenCode service from $cur back to its default (localhost)."
  note "This restarts the server once — your TUI will reconnect."
  run opencode service unset hostname || true
  run opencode service start || true
  if [[ "$OPT_DRY" == "1" ]]; then
    note "(dry-run: nothing changed)"
    return 0
  fi
  note "done: bind is now '$(svc_hostname)' (empty = localhost)"
  sleep 1
  local l
  l="$(ss -tlnH "sport = :$PORT" 2>/dev/null | awk '{print $4}' | tr '\n' ' ')"
  note "listening    : ${l:-none}"
  if [[ "$l" != *"127.0.0.1:$PORT"* ]]; then
    warn "the server does not appear to listen on 127.0.0.1:$PORT — check 'opencode service status'"
  fi
}


# --- KDE Connect helpers -----------------------------------------------------

kdeconnect_available() {
  command -v kdeconnect-cli >/dev/null 2>&1
}

find_kdeconnect_device() {
  local line id name
  [[ -n "${OPT_KDE_DEVICE:-}" ]] && { printf '%s' "$OPT_KDE_DEVICE"; return 0; }
  while IFS= read -r line; do
    id="$(printf '%s' "$line" | sed -nE 's/.*: ([a-f0-9]{32}).*/\1/p')"
    [[ -n "$id" ]] || continue
    # Prefer reachable devices; if not reachable, still return if it's the only one found
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
  local msg="$url"
  kdeconnect-cli -d "$dev" --share-text "$msg" >/dev/null 2>&1
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
    open|close|status|reset-bind|_tick|_lan_traffic) ;;
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
    reset-bind) cmd_reset_bind ;;
    _tick) cmd_tick ;;
    _lan_traffic) lan_traffic "${POSITIONALS[0]:-192.168.}"; echo ;;
  esac
}

main "$@"
