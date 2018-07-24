#ifdef Intl_FOUND
#include <libintl.h>
#define _(s) gettext(s)
#else
#define _(s) (s)
#endif
