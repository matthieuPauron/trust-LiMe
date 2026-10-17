#!/usr/bin/env bash
# phone-access.sh — temporary LAN + web access to the OpenCode server, for a phone.
#
#   phone-access.sh open  [--pair] [--idle 30m] [--max 4h]
#                         [--ip A.B.C.D] [--iface IFACE] [--subnet A.B.C.0/24]
#                         [--dry-run]
#   phone-access.sh close
#   phone-access.sh status
#   phone-access.sh _tick      (internal: called by the systemd user watchdog)
#
# What it does
#  - binds the shared OpenCode server to this machine's LAN address so a phone on
#    the same Wi-Fi can reach the web UI at http://<ip>:49374,
#  - opens a *tagged* ufw rule for that LAN subnet only (never the internet),
#  - optionally runs `opencode pair` for the one-time phone sign-in,
#  - arms a systemd user watchdog that closes everything after the idle timeout.
#
# How closing works
#  - the part that actually shuts the port is rebinding the service to localhost,
#    which needs no root; the tagged ufw rule is removed afterwards (root).
#  - the watchdog timer stays enabled at all times (a cheap 60 s tick) so that it
#    can also clean up a leftover tagged rule after a reboot or a crashed run.
#  - if the watchdog cannot use sudo non-interactively, it still closes the port
#    and marks the rule as pending; the next interactive `close`/`status`/`open`
#    removes it.
#
# Requires: iproute2 (ip, ss), curl, flock, ufw (active), systemd --user.
set -euo pipefail

SELF="$(readlink -f "${BASH_SOURCE[0]}")"
PORT=49374
TAG="opencode-phone"
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
OPT_PAIR=0
OPT_DRY=0
OPT_IDLE="$IDLE_DEFAULT"
OPT_MAX="0"
OPT_IP=""
OPT_IFACE=""
OPT_SUBNET=""
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
# key=value, rewritten atomically. Keys written by `open`, updated by `_tick`:
#   STATUS OPENED_AT LAST_ACTIVITY IDLE MAX IP IFACE SUBNET PREFIX
#   PEERS BYTES PREV_HOSTNAME UFW_RULE UFW_PENDING

state_init() { mkdir -p "$STATE_DIR"; [[ -f "$STATE" ]] || : >"$STATE"; }

state_get() {
  local key="$1"
  [[ -f "$STATE" ]] || return 0
  sed -n "s/^${key}=//p" "$STATE" | tail -1
}

# state_set KEY VALUE... (atomic: rewrite through a temp file)
state_set() {
  local key="$1"; shift
  local tmp="$STATE.tmp.$$"
  state_init
  { grep -v "^${key}=" "$STATE" 2>/dev/null || true; printf '%s=%s\n' "$key" "$*"; } >"$tmp"
  mv "$tmp" "$STATE"
}

state_clear() { : >"$STATE"; }

log_line() {
  state_init
  printf '%s %s\n' "$(date -Is)" "$*" >>"$LOG"
  tail -n 500 "$LOG" >"$LOG.cut" 2>/dev/null && mv "$LOG.cut" "$LOG" || true
}

# --- helpers: LAN address, ufw, opencode service ----------------------------

detect_ip() {
  local ip=""
  if [[ -n "$OPT_IFACE" ]]; then
    ip="$(ip -4 -o addr show "$OPT_IFACE" scope global 2>/dev/null \
          | awk 'NR==1{split($4,a,"/"); print a[1]}')"
    [[ -n "$ip" ]] || die "no IPv4 address on interface $OPT_IFACE"
  else
    ip="$(ip -4 route get 1.1.1.1 2>/dev/null \
          | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')"
    if [[ -z "$ip" ]]; then
      ip="$(ip -4 -o addr show scope global 2>/dev/null \
            | awk '{split($4,a,"/"); print a[1]}' \
            | grep -v '^172\.1[6-9]\.' | head -1)"
    fi
  fi
  [[ -n "$ip" ]] || die "cannot determine LAN address (pass --ip)"
  printf '%s' "$ip"
}

# Subnet prefix used to tell phone traffic from local (loopback) traffic.
ip_prefix() { local ip="$1"; printf '%s' "${ip%.*}."; }
subnet_of() { local ip="$1"; printf '%s' "${ip%.*}.0/24"; }

iface_of() {
  local ip="$1"
  ip -4 -o addr show scope global 2>/dev/null \
    | awk -v ip="$ip" '{split($4,a,"/"); if (a[1] == ip) {print $2; exit}}' \
    | sed 's/@.*//'
}

sudo_ok() { sudo -n true 2>/dev/null; }

# Is ufw running? Prefer the live status when sudo is already usable, otherwise
# fall back to the persisted ENABLED= setting and finally the service state, so
# we never prompt in a watchdog.
ufw_active() {
  if sudo_ok; then
    if sudo -n ufw status 2>/dev/null | head -1 | grep -qi 'inactive'; then return 1; fi
    if sudo -n ufw status 2>/dev/null | head -1 | grep -qi 'active'; then return 0; fi
  fi
  grep -qi '^ENABLED=yes' /etc/ufw/ufw.conf 2>/dev/null && return 0
  systemctl is-active --quiet ufw 2>/dev/null && return 0
  return 1
}

# auto="1" never prompts (watchdog); auto="0" may prompt for sudo.
ufw_has_tag() {
  local auto="$1" numbered
  if [[ "$auto" == "1" ]]; then
    sudo -n true 2>/dev/null || return 1
    numbered="$(sudo -n ufw status numbered 2>/dev/null || true)"
  else
    numbered="$(sudo ufw status numbered 2>/dev/null || true)"
  fi
  printf '%s\n' "$numbered" | grep -qF "$TAG"
}

ufw_add() {
  local ip="$1" iface="$2"
  local spec="allow in on $iface from ${ip%.*}.0/24 to any port $PORT proto tcp"
  run sudo ufw "$spec" comment "$TAG"
}

# Remove every rule tagged with our comment (handles duplicates/leftovers).
ufw_remove_tagged() {
  local auto="$1" numbered nums n
  if [[ "$auto" == "1" ]]; then
    sudo -n true 2>/dev/null || return 1
    numbered="$(sudo -n ufw status numbered 2>/dev/null || true)"
  else
    numbered="$(sudo ufw status numbered 2>/dev/null || true)"
  fi
  nums="$(printf '%s\n' "$numbered" | grep -F "$TAG" \
          | sed -E 's/^\[ *([0-9]+)\].*/\1/' | sort -rn || true)"
  [[ -z "$nums" ]] && return 0
  for n in $nums; do
    if [[ "$auto" == "1" ]]; then sudo -n ufw --force delete "$n" >/dev/null 2>&1 || true
    else sudo ufw --force delete "$n" >/dev/null 2>&1 || true; fi
  done
  return 0
}

svc_hostname() { opencode service get hostname 2>/dev/null || true; }
svc_set_hostname() { run opencode service set hostname "$1"; }
svc_unset_hostname() { run opencode service unset hostname; }
svc_start() { run opencode service start; }

# Is the server answering on this address/port? (any HTTP status counts)
probe() {
  curl -s -o /dev/null --max-time 4 "http://$1:$PORT/api/info"
}

# --- dry-run aware command execution ---------------------------------------

run() {
  if [[ "$OPT_DRY" == "1" ]]; then printf '  [dry-run] %s\n' "$*"; return 0; fi
  "$@"
}

# --- systemd user watchdog --------------------------------------------------

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
  note "Connect the phone (same Wi-Fi), either way:"
  note "  a) scan this QR code with the phone camera:"
  if command -v qrencode >/dev/null 2>&1 && [[ "$OPT_DRY" != "1" ]]; then
    qrencode -t ANSIUTF8 -m 1 "$url" | sed 's/^/     /'
  elif [[ "$OPT_DRY" == "1" ]]; then
    note "     [dry-run] would render a QR for $url"
  else
    note "     (qrencode not available)"
  fi
  note "  b) or open in the phone browser: $url"
  note "     username: opencode   password: \$(opencode service get password)"
  note "     then use 'Add to Home Screen' for an app-like icon."
  if [[ "$OPT_PAIR" == "1" ]]; then
    note ""
    note "One-time pairing link + QR (valid 5 min, single use):"
    run opencode pair --url "$url" || warn "opencode pair failed (continuing)"
  else
    note "  (add --pair on the first run to get a one-time sign-in link)"
  fi
}

cmd_open() {
  need opencode; need ip; need ss; need curl; need flock
  local ip iface subnet prefix prev_hostname idle_s max_s
  ip="${OPT_IP:-$(detect_ip)}"
  iface="${OPT_IFACE:-$(iface_of "$ip")}"
  subnet="${OPT_SUBNET:-$(subnet_of "$ip")}"
  prefix="$(ip_prefix "$ip")"
  idle_s="$(parse_duration "$OPT_IDLE")"
  max_s="$(parse_duration "$OPT_MAX")"
  [[ -n "$iface" ]] || die "cannot determine interface for $ip (pass --iface)"

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
    note "phone access was open on $prev_ip — reopening on $ip"
    do_close "reopen" "0" || true
  fi

  prev_hostname="$(svc_hostname)"
  note "== opencode phone access =="
  note "listen        : $ip:$PORT (interface $iface, subnet $subnet)"

  # 1. bind the shared service to the LAN address.
  svc_set_hostname "$ip" || true
  svc_start || true
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  (dry-run: skipping the reachability probe)"
  elif ! probe "$ip"; then
    warn "server did not answer on $ip — falling back to 0.0.0.0 (ufw still restricts to $subnet)"
    svc_set_hostname "0.0.0.0" || true
    svc_start || true
    probe "$ip" || { svc_unset_hostname || true; svc_start || true; die "server is not reachable on $ip:$PORT"; }
  fi
  note "listen        : answering on http://$ip:$PORT"

  # 2. firewall: LAN subnet only, and only on this interface. Never abort the
  #    open because of a firewall hiccup -- by now the service is already bound,
  #    so a warning has to be loud instead.
  local ufw_rule=0
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] add ufw rule: allow in on $iface from $subnet to any port $PORT proto tcp (tag $TAG)"
    ufw_rule=1
  elif command -v ufw >/dev/null 2>&1; then
    if ufw_add "$ip" "$iface"; then
      if ufw_has_tag 0; then
        ufw_rule=1
        note "firewall      : ufw rule added ($subnet -> tcp/$PORT in on $iface)"
      else
        warn "ufw accepted the rule but it is not visible in 'ufw status' — check manually"
      fi
    else
      warn "could not add the ufw rule (is ufw enabled?) — if the port is reachable now,"
      warn "anyone on $subnet can reach it; see 'sudo ufw status'"
      if ufw_active; then ufw_rule=1; fi
    fi
  else
    note "firewall      : ufw not installed, no rule added"
  fi

  # 3. state + watchdog.
  state_clear
  state_set STATUS open
  state_set OPENED_AT "$(now_epoch)"
  state_set LAST_ACTIVITY "$(now_epoch)"
  state_set IDLE "$idle_s"
  state_set MAX "$max_s"
  state_set IP "$ip"
  state_set IFACE "$iface"
  state_set SUBNET "$subnet"
  state_set PREFIX "$prefix"
  state_set PEERS ""
  state_set BYTES 0
  state_set PREV_HOSTNAME "$prev_hostname"
  state_set UFW_RULE "$ufw_rule"
  state_set UFW_PENDING 0
  install_units
  note "watchdog      : idle timeout $(fmt_duration "$idle_s")$(
    [[ "$max_s" != "0" ]] && printf ', hard stop after %s' "$(fmt_duration "$max_s")"
  ) (timer $(watchdog_state))"
  log_line "OPEN ip=$ip idle=${idle_s}s max=${max_s}s ufw_rule=$ufw_rule"

  print_connect_help "$ip"
  note ""
  note "Ctrl+C now, or just stop using the phone — either way everything closes."
  exec 9>&-            # release the lock: the watchdog must be able to tick
  if [[ "$OPT_DRY" == "1" ]]; then note "(dry-run: not waiting for the watchdog)"; return; fi
  wait_until_closed
}

# Stay in the foreground while the session is open, then report.
wait_until_closed() {
  local cleared=0
  trap 'cleared=1; do_close "interrupt" "0" || true; exit 0' INT TERM
  while [[ "$(state_get STATUS)" == "open" ]]; do
    sleep 5
  done
  [[ "$cleared" == "0" ]] || return 0
  print_close_summary
}

# --- close ------------------------------------------------------------------

print_close_summary() {
  local pending
  pending="$(state_get UFW_PENDING)"
  note "phone session closed"
  note "listen        : 127.0.0.1:$PORT (localhost only)"
  if [[ "$pending" == "1" ]]; then
    warn "ufw rule still present — run 'close' with sudo available to remove it"
  else
    note "firewall      : ufw rule removed (or none present)"
  fi
  note "watchdog      : timer $(watchdog_state) (keeps self-healing after reboot)"
  note ""
  note "The session itself keeps running here; press on with the TUI, or"
  note "run 'phone-access.sh open' again to work from the phone."
}

# auto: "1" when triggered by the watchdog (no interactive sudo); "0" otherwise.
do_close() {
  local reason="$1" auto="$2" prev_hostname status
  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || { warn "close: lock busy"; return 1; }

  status="$(state_get STATUS)"
  prev_hostname="$(state_get PREV_HOSTNAME)"

  # Nothing open and nothing to clean up? Then do not restart the service just
  # to "close" it -- that would blip the TUI for no reason.
  if [[ "$status" != "open" && "$OPT_DRY" != "1" ]]; then
    local has_tag=0 cur
    if ufw_has_tag "$auto"; then has_tag=1; fi
    cur="$(svc_hostname)"
    if [[ "$has_tag" == "0" && ( "$cur" == "127.0.0.1" || -z "$cur" ) ]]; then
      log_line "CLOSE no-op (already closed)"
      return 0
    fi
  fi

  # 1. rebind to localhost (this is what actually closes the port; no root).
  if [[ -n "$prev_hostname" ]]; then
    svc_set_hostname "$prev_hostname" || true
  else
    svc_unset_hostname || true
  fi
  svc_start || true

  # 2. drop our tagged ufw rule.
  local pending=0 want_rule
  want_rule="$(state_get UFW_RULE)"
  if [[ "$OPT_DRY" == "1" ]]; then
    note "  [dry-run] remove ufw rules tagged $TAG"
  elif [[ "$auto" == "1" ]] && ! sudo -n true 2>/dev/null; then
    # The watchdog cannot escalate; if a rule was added, it must be cleaned up
    # later by an interactive `close`/`status`/`open` (or with a sudoers rule).
    if [[ "$want_rule" == "1" ]]; then pending=1; fi
  else
    if ufw_has_tag "$auto"; then
      ufw_remove_tagged "$auto" || pending=1
    fi
    if [[ "$pending" == "0" ]] && ufw_has_tag "$auto"; then pending=1; fi
  fi

  state_set STATUS closed
  state_set PEERS ""
  state_set BYTES 0
  state_set UFW_PENDING "$pending"
  state_set LAST_ACTIVITY "$(now_epoch)"
  log_line "CLOSE reason=$reason (was=$status) ufw_pending=$pending"
  return 0
}

cmd_close() {
  need opencode; need flock
  do_close "manual" "0"
  print_close_summary
}

# --- watchdog tick ----------------------------------------------------------

cmd_tick() {
  need flock
  mkdir -p "$STATE_DIR"
  exec 9>"$LOCK"; flock -w 5 9 || exit 0

  local status
  status="$(state_get STATUS)"

  # Not open: only job is to heal a leftover tagged rule.
  if [[ "$status" != "open" ]]; then
    if ufw_has_tag 1; then
      if ufw_remove_tagged 1; then
        log_line "HEAL removed leftover ufw rule"
      else
        state_set UFW_PENDING 1
        log_line "HEAL deferred: ufw rule needs interactive sudo"
      fi
    fi
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
    do_close "idle" "1" || true
    exit 0
  fi
  if [[ -n "$max" && "$max" != "0" ]] && (( now - ${opened:-$now} > max )); then
    log_line "AUTO-CLOSE hard stop after $((now - ${opened:-$now}))s (limit ${max}s)"
    do_close "max-lifetime" "1" || true
    exit 0
  fi
  exit 0
}

# --- status -----------------------------------------------------------------

cmd_status() {
  need opencode
  local status ip idle last_act peers bytes opened max pend
  status="$(state_get STATUS)"
  ip="$(state_get IP)"

  if [[ "$status" != "open" ]]; then
    local cur
    cur="$(svc_hostname)"
    note "phone access : CLOSED (listening on ${cur:-127.0.0.1}:$PORT)"
    if state_is_set; then
      note "last change  : $(date -d "@$(state_get LAST_ACTIVITY)" '+%H:%M' 2>/dev/null || echo '?')"
    fi
    note "ufw rule     : $(ufw_status_note)"
    note "watchdog     : timer $(watchdog_state)"
    return 0
  fi

  idle="$(state_get IDLE)"; last_act="$(state_get LAST_ACTIVITY)"
  peers="$(state_get PEERS)"; opened="$(state_get OPENED_AT)"; max="$(state_get MAX)"
  pend="$(state_get UFW_PENDING)"
  local now idle_for
  now="$(now_epoch)"; idle_for=$(( now - last_act ))

  note "phone access : OPEN since $(date -d "@$opened" '+%H:%M' 2>/dev/null || echo "$opened")"
  note "listen       : $(svc_hostname):$PORT   (phone: http://$ip:$PORT)"
  note "interface    : $(state_get IFACE)  subnet $(state_get SUBNET)"
  note "phone peers  : ${peers:-none}   ($(state_get BYTES) bytes seen)"
  note "last activity: $(( now - last_act ))s ago"
  note "auto-close   : in $(fmt_duration $(( idle - idle_for ))) of idle$(
    [[ -n "$max" && "$max" != "0" ]] && printf ' (hard stop in %s)' "$(fmt_duration $(( max - (now - opened) )))"
  )"
  note "ufw rule     : $(ufw_status_note)"
  [[ "$pend" == "1" ]] && warn "a leftover ufw rule is pending removal"
  note "watchdog     : timer $(watchdog_state)"
  note "log          : $LOG"
}

state_is_set() { [[ -s "$STATE" ]]; }

ufw_status_note() {
  if ! ufw_active; then printf 'ufw inactive\n'; return; fi
  if sudo_ok; then
    if sudo -n ufw status numbered 2>/dev/null | grep -qF "$TAG"; then
      printf 'present (tagged %s)\n' "$TAG"
    else
      printf 'none\n'
    fi
  else
    printf 'unknown (needs sudo)\n'
  fi
}

# --- wiring -----------------------------------------------------------------

usage() {
  sed -n '2,27p' "$SELF" | sed 's/^# \{0,1\}//'
  exit "${1:-2}"
}

parse_args() {
  [[ $# -gt 0 ]] || usage
  CMD="$1"; shift
  case "$CMD" in
    open|close|status|_tick|_lan_traffic) ;;
    -h|--help|help) usage 0 ;;
    *) die "unknown command: $CMD" ;;
  esac
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --pair) OPT_PAIR=1 ;;
      --dry-run) OPT_DRY=1 ;;
      --idle) OPT_IDLE="${2:?--idle needs a value}"; shift ;;
      --max) OPT_MAX="${2:?--max needs a value}"; shift ;;
      --ip) OPT_IP="${2:?--ip needs a value}"; shift ;;
      --iface) OPT_IFACE="${2:?--iface needs a value}"; shift ;;
      --subnet) OPT_SUBNET="${2:?--subnet needs a value}"; shift ;;
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
    _lan_traffic) lan_traffic "${POSITIONALS[0]:-192.168.}"; echo ;;
  esac
}

main "$@"
