#include "export.h"

static bool exporting = false;

// export rendering
bool export_rendering() {
  return exporting;
}

// start export
void export_start() {
  exporting = true;
}

// done export
void export_done() {
  exporting = false;
}