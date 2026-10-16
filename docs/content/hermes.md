@page TrustLiMe_hermes Using Hermes Agent

This page is a hands-on tutorial for the **Hermes Agent** installation that
supports TRUST-LiMe development on the workstation. It explains what Hermes adds
on top of the OpenCode coding agent, how it is wired for this repository, and the
handful of commands needed day to day.

@note Hermes is a *local development aid*. Nothing on this page is required to
compile the module, to build this documentation, or to run the validation cases.

---

## 1. What Hermes is, and what it is not

TRUST-LiMe is developed interactively with **OpenCode** (see the project
`AGENTS.md`). Hermes Agent is a second, complementary runtime: a long-running
agent that can work **without a human at the keyboard**. It does not replace
OpenCode, and it does not touch the module's build.

| Layer | Tool |
|---|---|
| Interactive editing, builds, validation | OpenCode in this repository |
| Scheduled / unattended runs | Hermes cron |
| Delegated coding | Hermes calling `opencode run` (bundled `opencode` skill) |
| Long-term memory and reusable skills | Hermes memory, skills and curator |
| Model selection | Hermes primary and fallback providers |

Three properties make it useful for this project:

1. **It runs on a schedule** — the nightly arithmetic regression guard
   (section @ref TrustLiMe_hermes_job "The nightly regression guard").
2. **It remembers** — findings accumulate in a memory file and in
   agent-authored skills, instead of being re-derived in every session.
3. **It is model-agnostic** — one fast primary model with an automatic free
   fallback when the primary rate-limits.

---

## 2. The installation on this machine

Hermes is installed per user, not per project:

| Piece | Location |
|---|---|
| Install directory | `~/.hermes/hermes-agent` (git install, `hermes update` to refresh) |
| Main configuration | `~/.hermes/config.yaml` |
| Provider keys | `~/.hermes/.env` |
| Memory file | `~/.hermes/memories/MEMORY.md` |
| Skills | `~/.hermes/skills/` |
| Cron jobs | `~/.hermes/cron/jobs.json` |
| Logs | `~/.hermes/logs/agent.log` |

Check the version and the overall health at any time:

```bash
hermes --version
hermes doctor
hermes status
```

`hermes doctor` prints a per-component health report and a short numbered list of
issues. Optional integrations (Discord, xAI search, image generation) will show
as warnings if their keys are not configured; that is expected and harmless for
this project.

### Model routing

The configuration is deliberately minimal:

| Setting | Value |
|---|---|
| `model.provider` | `gemini` |
| `model.default` | `gemini-3.1-flash-lite` |
| `fallback_providers` | `nvidia/nemotron-3.5-lightning:free` (via OpenRouter) |

The primary is fast; when it fails with a rate-limit, overload or connection
error, Hermes automatically retries on the fallback. Inspect or edit the chain
with:

```bash
hermes fallback list     # show primary + ordered fallbacks
hermes fallback add      # pick an extra fallback interactively
hermes fallback remove
```

@warning The Gemini key used here is on the **free tier**. A single agent turn
can cost several model calls, so a long interactive session may exhaust the daily
quota and spill onto the fallback — or, once both are spent, fail. For heavy
interactive work prefer OpenCode; keep Hermes for scheduled and delegated jobs.

### Why a model is pinned on delegated runs

Some models reachable through the NVIDIA endpoint are unprovisioned or hang for
this account. Any Hermes job that shells out to OpenCode should therefore pin a
known-good model explicitly instead of relying on a provider default:

```bash
opencode run --provider gemini --model gemini-3.1-flash-lite "..."
```

---

## 3. Everyday commands

```bash
hermes                       # interactive chat (long-lived session)
hermes -z "summarise the failing test"   # one-shot prompt, then exit
hermes model                 # pick the default provider/model interactively
hermes config get model      # resolved values (flattened, with defaults)
hermes config set model.default gemini-3.1-flash-lite
hermes config unset model.base_url
hermes config check          # flag missing or deprecated keys
hermes logs                  # view and filter the agent log
```

@note A one-shot run (`-z`) exits before the background self-improvement review
can finish. That review only completes in a long-lived session — the interactive
`hermes` console or the messaging gateway.

---

## 4. The nightly regression guard {#TrustLiMe_hermes_job}

The concrete win of the Hermes setup is a cron job that runs the fast arithmetic
unit-test suite every night and reports failures without anyone logging in:

| Property | Value |
|---|---|
| Name | `trust-lime-arith-nightly` |
| Schedule | `0 3 * * *` (03:00 local) |
| Script | `~/.hermes/scripts/trust-lime-arith-tests.sh` |
| Workdir | the TRUST-LiMe repository root |
| Model | `gemini-3.1-flash-lite` (pinned on the job itself) |
| Delivery | `local` (failures only) |

The script runs `.opencode/scripts/unit-tests.sh --arith-only` and prints a
compact `RESULT: PASS|FAIL|TIMEOUT` summary. In *agent mode*, Hermes injects that
output into the job's prompt, so the model reports which tests failed when the
result is not a pass.

Manage and inspect it with:

```bash
hermes cron list                       # all scheduled jobs
hermes cron status                     # is the scheduler running?
hermes cron runs trust-lime-arith-nightly   # durable execution history
hermes cron doctor                     # health checks across jobs
hermes cron run <id>                   # run on the next scheduler tick
hermes cron pause <id>                 # stop one job
hermes cron notepad <id>               # persistent key/value notes across runs
```

To widen the nightly coverage, extend `trust-lime-arith-tests.sh` (it is a plain
Bash script). Keep it to **fast** checks — the per-file diagnostics loop and the
arithmetic suite — and leave full-kernel builds out of cron, since those are slow
on every development host.

@note `hermes verify` detects a project's build and test recipe and smoke-tests
it. A project may declare that recipe explicitly in `.hermes/environment.json`;
without it, Hermes falls back to generic `Makefile` detection.

---

## 5. Memory, skills and the curator

Hermes improves over time in two ways:

- **Memory** — durable notes live in `~/.hermes/memories/MEMORY.md`, written
  after sessions and nudged every few turns.
- **Skills** — reusable procedures are authored as Markdown under
  `~/.hermes/skills/`. Agent-created skills are marked with their provenance.

Both use free writes (no approval prompt), so the loop runs unattended. A
background **curator** periodically reviews skills, prunes stale ones and keeps
provenance; it never deletes memories.

```bash
hermes memory status              # which memory provider is active
hermes skills list                # installed skills
hermes skills search <query>      # search skill registries
hermes skills inspect <name>      # preview before installing
hermes curator status             # curator state and skill statistics
hermes curator run                # trigger a review now
hermes curator pin <skill>        # protect a skill from auto-transition
```

### Project-local skills

A repository can ship its own skills in `./.hermes/skills/` (or
`./.agents/skills/`). They load only once the repository is trusted:

```bash
hermes skills trust               # trust the current repo's project skills
hermes skills untrust             # revoke
```

---

## 6. Delegating to OpenCode

Hermes is the scheduler; OpenCode is the coding worker. A job or an interactive
prompt can ask Hermes to shell out to OpenCode in this repository — for example,
to triage a failing test or to open a worktree and prepare a patch. Two rules
from the project `AGENTS.md` apply:

- **Pin the model** on the delegated call (section 2).
- **Never let it push.** The project denies `git push` to agent sessions; keep
  that policy.

```bash
hermes worktree                  # audit and reclaim accumulated git worktrees
hermes project show trust-lime   # folders bound to this project
```

The project binding assigns TRUST-LiMe **and** its sibling `trust-code` checkout,
so a delegated job can read the kernel base classes it needs:

```
trust-lime  [p_85384aa4]
  primary: /home/matthieu/Development/trust-LiMe
  folders:
    * /home/matthieu/Development/trust-LiMe
      /home/matthieu/Development/trust-code
```

---

## 7. Stopping, resetting and rolling back

```bash
hermes pause      # emergency stop: halt cron/kanban dispatch and new gateway turns
hermes resume     # lift the emergency stop
```

`hermes pause` is the single switch that silences all scheduled and gateway
activity. Use it before long rebuilds, or whenever you want the machine quiet.

Configuration is easy to restore, because Hermes writes timestamped backups:

| Backup | Location |
|---|---|
| Configuration | `~/.hermes/backups/config/config.yaml.bak.<timestamp>` |
| Provider keys | `~/.hermes/.env.bak.<timestamp>` |

A manual snapshot before an experimental change is equally cheap:

```bash
cp ~/.hermes/config.yaml ~/.hermes/config.yaml.mybackup
```

@warning Editing `~/.hermes/config.yaml` by hand is fine, but prefer
`hermes config set` / `unset`: they preserve the file's comments and clear stale
values. Note that the string `none` is written as a null value, leaving a bare
key.

---

## 8. Troubleshooting

| Symptom | First response |
|---|---|
| Any failure | `hermes doctor`, then `hermes logs` |
| A session hangs on a reply | the pinned model is unhealthy — switch with `hermes model` or `--model` |
| Config looks wrong | `hermes config check`; compare `hermes config get <key>` with the file |
| A key seems missing | `hermes config env-path`, then check the file it prints |
| Cron job never fires | `hermes cron status`, then `hermes cron doctor` |
| Runaway scheduled work | `hermes pause` |

The three failure modes seen most often here:

1. **Free-tier exhaustion.** Both the primary and the fallback are free lanes, so
   a burst of auxiliary calls can starve a session. The cure is fewer calls, not
   a bigger prompt: keep interactive work in OpenCode.
2. **An unhealthy model.** A model that is unprovisioned for an account can hang
   rather than error. Pin a known-good model on every delegated run.
3. **The review never runs.** The self-improvement review is a background thread;
   it only completes in a long-lived session, not in `-z` one-shots.

---

## 9. Cheat sheet

| Goal | Command |
|---|---|
| Interactive session | `hermes` |
| One-shot prompt | `hermes -z "..."` |
| Pick the model | `hermes model` |
| Show fallbacks | `hermes fallback list` |
| Health report | `hermes doctor`, `hermes status` |
| Read or set config | `hermes config` (`get`/`set`/`unset`/`check`) |
| List cron jobs | `hermes cron list` |
| Job execution history | `hermes cron runs <id>` |
| Run a job now | `hermes cron run <id>` |
| Project folders | `hermes project show trust-lime` |
| Skills | `hermes skills` (`list`/`search`/`install`) |
| Curator | `hermes curator` (`status`/`run`) |
| Memory | `hermes memory status` |
| Usage and limits | `hermes usage`, `hermes insights` |
| Emergency stop | `hermes pause` |

---

@note Everything on this page was verified against **Hermes Agent v0.21.7**
(`hermes --version`) on the workstation. Run `hermes --help` for the full,
authoritative command list.
