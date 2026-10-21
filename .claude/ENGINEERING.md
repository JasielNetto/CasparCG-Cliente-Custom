# Manual de Engenharia de Software

Este documento define os padrões gerais de desenvolvimento do projeto. As regras abaixo orientam o agente, salvo requisitos explícitos do usuário ou do projeto.

## Princípios

Priorize simplicidade, clareza, legibilidade, manutenibilidade, segurança, reutilização e consistência.

Antes de criar algo novo, procure solução existente. Evite duplicação e complexidade sem benefício claro.

## Contexto

O projeto deve possuir `PROJECT_CONTEXT.md` como memória técnica. Registre decisões arquiteturais, contexto de negócio, convenções, limitações, integrações e outros fatos necessários à continuidade.

Consulte e atualize o contexto somente quando houver relação com a tarefa. Não registre informações temporárias nem duplique o README.

## Comunicação

A comunicação com o usuário, comentários de código e documentação interna devem ser em Português (Brasil), salvo necessidade explícita de inglês.

Explique decisões técnicas relevantes. Ao solicitar autorização para comandos, informe objetivo, impacto, arquivos afetados e riscos quando aplicável.

## Pensamento crítico

Avalie coerência, consistência e plausibilidade das premissas. Se houver inconsistência, informe-a e apresente alternativas. Não invente informações. Quando uma hipótese for necessária, deixe-a explícita.

## Processo

Para alterações médias ou grandes, compreenda o problema, valide requisitos, avalie arquitetura/riscos, defina abordagem e divida a execução antes de implementar.

Não faça planejamento formal para tarefas simples ou para decisões já estabelecidas.

## Git

Use Git durante o desenvolvimento. Verifique o estado do repositório antes de alterações relevantes e revise `git diff`/`git status` ao finalizar uma alteração lógica.

Utilize Conventional Commits (`feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `chore`). Cada commit deve representar uma alteração lógica completa.

Não faça push automaticamente. Não execute `reset`, `rebase`, `clean`, force push ou exclusão de branch sem confirmação explícita.

Branches usuais: `main`, `develop`, `feature/*`, `fix/*`, `refactor/*`, `docs/*`, `experiment/*`, `hotfix/*`.

## Código

Priorize responsabilidade única, baixo acoplamento, alta coesão, nomes claros e funções/classes objetivas.

Evite código morto, duplicação, comentários redundantes, valores mágicos e configurações hardcoded. Centralize configurações quando apropriado.

## Dependências

Antes de adicionar biblioteca, avalie necessidade, manutenção, licença, impacto e alternativas. Evite dependências desnecessárias.

## Testes

Para alterações relevantes, indique como validar, resultado esperado, casos positivos/negativos e limitações. Crie testes automatizados quando fizer sentido.

## Documentação

Mantenha a documentação afetada pela alteração sincronizada. Atualize somente os documentos realmente necessários, como README, CHANGELOG, arquitetura, API, instalação, desenvolvimento, deploy e roadmap.

## Ambiente e deploy

Quando aplicável, documente dependências, versões, pré-requisitos, variáveis de ambiente, instalação, build, empacotamento, deploy, rollback e dependências externas.

## Internacionalização

Evite textos de interface diretamente no código. Centralize mensagens quando houver necessidade de múltiplos idiomas. Não introduza infraestrutura de i18n sem necessidade do projeto.

## Segurança

Nunca coloque senhas, tokens, API keys, credenciais ou certificados no código. Valide entradas externas e evite informações sensíveis em logs.

## Revisão

Para alterações relevantes, revise bugs, duplicações, arquitetura, desempenho, segurança, testes e documentação. Sugira melhorias antes de grandes refatorações fora do escopo.

## Finalização

Ao concluir uma tarefa relevante, apresente resumo, arquivos modificados, validação realizada, limitações e mensagem de commit sugerida.

Priorize qualidade sem introduzir complexidade desnecessária. Quando velocidade e qualidade entrarem em conflito, siga a prioridade explícita do usuário.
