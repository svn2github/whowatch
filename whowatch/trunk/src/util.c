#include "config.h"
#include <err.h>
#include <stdlib.h>
#include <sys/time.h>
#include "whowatch.h"

#ifndef RETURN_TV_IN_SELECT

/* Taken from the glibc manual */

/*
  Subtract the `struct timeval' values X and Y,
  storing the result in RESULT.
  Return 1 if the difference is negative, otherwise 0.
*/
static int timeval_subtract (struct timeval *result,
			     struct timeval *x,
			     struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating Y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait. tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

#undef select
int _select (int nfds, fd_set *readfds, fd_set *writefds,
	     fd_set *exceptfds, struct timeval *timeout)
{
  struct timeval before;
  struct timeval after;
  struct timeval delta;
  int retval;

  gettimeofday (&before, 0);
  retval = select (nfds, readfds, writefds, exceptfds, timeout);
  gettimeofday (&after, 0);
  if (timeval_subtract (&delta, &after, &before) == 0) {
    // after >= before
    if (timeval_subtract (timeout, timeout, &delta) == 1) {
      // *timeout < 0
      timeout->tv_sec = 0;
      timeout->tv_usec = 0;
    }
  }
  return retval;
}

#endif /* !RETURN_TV_IN_SELECT */

void* xmalloc (size_t size)
{
  void *ptr = malloc (size);
  if (ptr == NULL) {
    err (EXIT_FAILURE, NULL);
  }
  return ptr;
}

void* xcalloc (size_t nmemb, size_t size)
{
  void *ptr = calloc (nmemb, size);
  if (ptr == NULL) {
    err (EXIT_FAILURE, NULL);
  }
  return ptr;
}

void *xrealloc (void *ptr, size_t size)
{
  ptr = realloc (ptr, size);
  if (ptr == NULL) {
    err (EXIT_FAILURE, NULL);
  }
  return ptr;
}
