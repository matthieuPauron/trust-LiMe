---
description: Check validation-case formatting rules (R1-R3)
---
Run the validation-case formatting gate and report the result:

    .opencode/scripts/check-validation.sh

It enforces the hard formatting rules (canonical text: the
`trust-lime-notebooks` skill; summary in AGENTS.md):

- **R1** every `.ipynb` lives at the case root
  (`share/Validation/Rapports_automatiques/<Case>/<Case>.ipynb`), never in
  `src/`;
- **R2** no `.data` contains TRUST parallelism machinery (`Decouper`,
  `Partitionneur`, `Partition`, `Partition_tool`, `Scatter`, `Larg_joint`,
  `Nom_Zones`, `zones_name`);
- **R3** no notebook calls `case.partition()`, sets `nbProcs > 1`, or passes
  `trust -partition`.

Exit 0 = clean. On non-zero, list each `file:line` violation and fix it:
strip the `Decouper { … } Fin` + `Scatter` block from the dataset, move the
notebook to the case root, and drop the parallel options from the notebook.
Do not weaken the check to make it pass.
