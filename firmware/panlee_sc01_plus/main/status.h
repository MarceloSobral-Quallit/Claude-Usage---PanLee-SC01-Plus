#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ok;
    bool haikuUp;
    bool sonnetUp;
    bool opusUp;
    bool fableUp;
} ModelStatus;

/* GET em STATUS_ENDPOINT (status.claude.com); um modelo e considerado "up"
 * quando seu nome NAO aparece entre os incidentes nao resolvidos. Em caso
 * de falha de rede, retorna false e mantem o ultimo estado conhecido no
 * chamador (nao sobrescreve out). */
bool fetchModelStatus(ModelStatus *out);

#ifdef __cplusplus
}
#endif
