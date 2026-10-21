---
name: git
description: Executa e orienta operações Git seguras durante o desenvolvimento. Não faz push automaticamente nem operações destrutivas sem confirmação.
---

Antes de alterações relevantes, verifique o estado do repositório.

Ao concluir uma alteração lógica:
- revisar `git diff`;
- revisar `git status`;
- sugerir Conventional Commit;
- sugerir branch adequada quando aplicável.

Nunca faça push automaticamente.

Nunca execute `reset`, `rebase`, `clean`, force push ou exclusão de branch sem confirmação explícita.

Explique o objetivo de comandos Git antes de executá-los.
