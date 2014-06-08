/*
 * File:   TicToc.h
 * This is a slight hack (this file is borrowed from HAL) but it should work
 */

#ifndef NODE_TICTOC_H
#define NODE_TICTOC_H

#include <sys/time.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach_time.h>
#endif

namespace node_time
{

////////////////////////////////////////////////////////////////////////////////
// Aux Time Functions
inline double Tic()
{
#ifdef __MACH__
  // From Apple Developer Q&A @
  // https://developer.apple.com/library/mac/qa/qa1398/_index.html

  // This doesn't change, so we set it up once
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    mach_timebase_info(&timebase);
  }

  double secs = static_cast<double>(mach_absolute_time()) *
      timebase.numer / timebase.denom * 1e-9;
  return secs;
#elif _POSIX_TIMERS > 0
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + 1e-6 * (tv.tv_usec);
#endif
}

////////////////////////////////////////////////////////////////////////////////
inline double RealTime()
{
    return Tic();
}

////////////////////////////////////////////////////////////////////////////////
inline double Toc( double  dTic )
{
    return Tic() - dTic;
}

////////////////////////////////////////////////////////////////////////////////
inline double TocMS( double  dTic )
{
    return ( Tic() - dTic )*1000.;
}

}

#endif	/* NODE_TICTOC_H */
