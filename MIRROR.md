# Mirror (substitui o "Shadow server")

O Mirror replica **PLAY, LOAD e STOP** do Servidor/Canal de origem (ex.: S1/C1)
para o Servidor/Canal de destino (ex.: S2/C2), no nivel de transporte AMCP.

* Verbos padrao: `PLAY`, `LOAD` (inclui `LOADBG`), `STOP`. Opcionais (engrenagem):
  `PAUSE`, `RESUME`, `CLEAR`, `MIXER`, `CALL`, `CG`.
* O canal de origem e trocado pelo canal de destino; a layer e preservada.
* Os dois servidores sao independentes: se o destino cair, o comando e descartado
  (e contado) e o servidor de origem continua normalmente. Se a origem cair,
  os comandos continuam indo para o destino (`AmcpDevice::canSend()`).
* Barra de menu (canto direito): LEDs verde/vermelho dos dois servidores, chave
  **Mirror** e engrenagem de configuracao. Tooltip da chave mostra espelhados/descartados.
* Configuracao salva na tabela `Configuration` (`MirrorEnabled`, `MirrorSourceDevice`,
  `MirrorSourceChannel`, `MirrorTargetDevice`, `MirrorTargetChannel`, `MirrorVerbs`);
  banco versao 218.

## Mudancas em relacao ao Shadow
* Os ~173 laços de duplicacao por widget foram removidos. Quem usava Shadow deve
  configurar o Mirror.
* A caixa em Settings > Devices agora se chama "Hidden server": apenas oculta o servidor
  da biblioteca/inspector (comportamento anterior do flag).
* O espelhamento do canal de preview nao e mais feito pelo Shadow.

## Testes
`g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I src/Caspar tools/tests/mirror_translator_test.cpp -o mirror_test && ./mirror_test`

## Aviso de build
Licenca GPLv3 (herdada do CasparCG Client). O codigo Qt/C++ novo (MirrorManager, dialogo,
widget, integracao) nao foi compilado no ambiente de desenvolvimento; compile via GitHub
Actions (`.github/workflows/windows.yaml`) ou Qt 6.5 + VS2022 + CMake.
