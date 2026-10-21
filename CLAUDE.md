# Claude Workspace

## Regra principal

Antes de qualquer atividade, leia `.claude/ENGINEERING.md`.

As regras de `ENGINEERING.md` são obrigatórias e têm prioridade sobre preferências do agente, exceto requisitos explícitos do usuário ou do projeto.

## Contexto do projeto

`PROJECT_CONTEXT.md` contém decisões e contexto técnico do projeto.

Consulte-o somente quando a tarefa depender de decisões anteriores, requisitos já definidos, comportamento existente ou arquitetura/integrações.

Após uma decisão técnica relevante, atualize-o quando necessário.

## Skills

Utilize somente a skill necessária para a tarefa. Não carregue skills por precaução.

Disponíveis: `planning`, `implementation`, `debugging`, `documentation`, `git`, `release`.

## Agentes

- `architect`: somente para decisões ou dúvidas arquiteturais relevantes. Não use quando a decisão já estiver definida.
- `reviewer`: antes de concluir alterações relevantes de código, arquitetura, governança ou documentação. Não use para tarefas triviais.

## Regras de operação

- Não explore o projeto inteiro sem necessidade.
- Comece pelos arquivos diretamente relacionados à tarefa.
- Leia somente o contexto necessário.
- Evite refatorações fora do escopo e alterações especulativas.
- Faça mudanças incrementais e verificáveis.
- Preserve decisões arquiteturais existentes.
- Não altere arquivos fora do escopo explícito da tarefa.

## Modos de trabalho

### Spike
Investigue e valide a hipótese. Evite implementação definitiva e abstrações desnecessárias. Registre resultados e decisões relevantes.

### Implementação
Identifique os pontos de integração, altere somente o necessário, valide e revise antes de concluir quando a mudança for relevante.

### Debugging
Reproduza ou localize o problema, investigue evidências, identifique a causa provável, faça a correção mínima e valide.

## Finalização

Antes de concluir uma tarefa relevante:

1. valide as alterações;
2. execute testes/verificações pertinentes;
3. revise `git diff` e `git status`;
4. atualize documentação/contexto quando necessário;
5. informe limitações e próximos passos.

Nunca faça push automaticamente.

## Governança

Se `ENGINEERING.md`, `PROJECT_CONTEXT.md`, uma skill ou um agent for alterado, revise a consistência dessas regras antes de continuar o desenvolvimento.
