#include "config.h"
#include "whowatch.h"

int read_key ()
{
  int c;

  c = wgetch (current->wd);

  if (c == '\n') {
    return KEY_ENTER;
  }

  return c;
}
