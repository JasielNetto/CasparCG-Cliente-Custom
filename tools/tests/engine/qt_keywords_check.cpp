// Garante que os cabeçalhos do núcleo compilam dentro de código Qt.
// O Qt define emit/signals/slots/foreach como macros; um nome igual no núcleo quebra o build do
// cliente sem quebrar os testes (que não usam Qt). Este arquivo simula essas macros.

#define emit
#define signals public
#define slots
#define foreach(a, b) for (a : b)
#define Q_SIGNALS public
#define Q_SLOTS
#define Q_EMIT

#include "AmcpFormat.h"
#include "ClipEndDetector.h"
#include "Conjunto.h"
#include "Item.h"
#include "PlayoutPort.h"
#include "Resource.h"
#include "Rundown.h"
#include "Sequence.h"
#include "StationConfig.h"
#include "TimeFormat.h"
