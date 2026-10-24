#!/usr/bin/env bash
# Source the TRUST + trust-LiMe environment, then exec the given command.
#
# Usage:
#   .opencode/scripts/with-trust-env.sh <command> [args...]
#   .opencode/scripts/with-trust-env.sh bash -c 'make module_semi_optim && ...'
#
# Overrides (env vars):
#   TRUST_ROOT   (default: /home/matthieu/Development/trust-code)
#   VENV_TRUST   (default: /home/matthieu/Development/venv_trust)
# Do not use strict mode: the TRUST env scripts rely on unset variables and
# commands that may return non-zero (e.g. grep with no match).
TRUST_ROOT="${TRUST_ROOT:-/home/matthieu/Development/trust-code}"
VENV_TRUST="${VENV_TRUST:-/home/matthieu/Development/venv_trust}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# shellcheck disable=SC1091
source "$TRUST_ROOT/env_TRUST.sh"
# shellcheck disable=SC1091
source "$TRUST_ROOT/env_for_python.sh"
# shellcheck disable=SC1091
source "$VENV_TRUST/bin/activate"
# shellcheck disable=SC1091
source "$REPO_ROOT/env_trust_LiMe.sh"

exec "$@"