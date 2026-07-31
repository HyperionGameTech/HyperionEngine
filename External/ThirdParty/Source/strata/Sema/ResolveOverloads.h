#pragma once

#include "strata/AST/AST.h"
#include "strata/Core/Diagnostics.h"

void ResolveOverloads(Module* mod, DiagnosticEngine* diag, Arena* arena);
