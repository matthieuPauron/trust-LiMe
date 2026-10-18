@mainpage TRUST-LiMe Documentation

**TRUST-LiMe** is a *baltik* application: a C++ module that extends the CEA
[TRUST](https://cea-trust-platform.github.io/) thermohydraulic platform with a
**magnetic induction equation**, so that a magnetic field can be transported and
diffused on top of the existing thermohydraulics. The long-term goal is a code
able to natively perform coupled turbulent MHD (and, eventually, MHD coupled
with neutronics) for the study of breeder layers in stellarator reactors.

TRUST-LiMe is a *module*, not a fork: the kernel sources live in a sibling
checkout of `trust-code`, and this repository only contains the module's own
sources (`src/`), its tests (`tests/`, `share/Validation/`), and this
documentation. Reusing the TRUST kernel gives the module its discretisation,
time integration, data-file parsing, and boundary/source machinery for free.

---

- @subpage TrustLiMe_intro
- @subpage TrustLiMe_model
- @subpage TrustLiMe_testcases
- @subpage TrustLiMe_validation
- @subpage TrustLiMe_plans
- @subpage TrustLiMe_api
- @subpage TrustLiMe_docsys
- @subpage TrustLiMe_hermes
- @subpage KeywordsReference

---

<h2>Quick start</h2>

TRUST-LiMe builds through the baltik framework, on top of an
already-compiled TRUST kernel:

```bash
source env_trust_LiMe.sh   # exports TRUST_ROOT, project_directory, exec_opt, ...
make module_semi_optim     # rebuild just this module (-O3, asserts on)
```

Then generate this documentation — independently of the build:

```bash
make docs
# or directly:
cd docs && make
```

The HTML output is written to `docs/build/html/index.html`.

<h2>Where to go next</h2>

- New to the project? Start with the @ref TrustLiMe_intro "Introduction".
- Looking for what the module solves? See
  @ref TrustLiMe_model "Problems, equations and boundary conditions".
- Want to run it? See @ref TrustLiMe_testcases "Test cases" and
  @ref TrustLiMe_validation "Validation cases".
- Planning ahead? See @ref TrustLiMe_plans "Roadmap and next steps".
- Extending the code? Browse the @ref TrustLiMe_api "C++ API reference".
- Writing documentation? See @ref TrustLiMe_docsys "Documentation system".
- Automating checks or running the agent unattended? See
  @ref TrustLiMe_hermes "Using Hermes Agent".

<h2>Credits</h2>

**TRUST-LiMe** is built on top of the [TRUST platform](https://cea-trust-platform.github.io/),
an open-source thermohydraulic framework developed at CEA (DES/ISAS/DM2S/SGLS/LCAN).

- TRUST GitHub: <https://github.com/cea-trust-platform/trust-code>
- TRUST documentation: <https://cea-trust-platform.readthedocs.io/en/latest/>
- TRUST-LiMe GitHub: <https://github.com/matthieuPauron/trust-LiMe>

This page is powered by [Doxygen](https://www.doxygen.nl/) and
[doxygen-awesome](https://github.com/jothepro/doxygen-awesome-css), through the
shared TRUST *baltik* documentation pipeline.
