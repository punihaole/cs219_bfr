/*
 * time.h
 *
 * This file contains the functions needed to keep track of time. We use time-
 * stamps to manage the PIT and CS (and possibly other purposes). This header
 * shall be a wrapper for a device/OS/whatever-dependent solution.
 *
 */

#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

int timestamp();

#endif // TIME_H_INCLUDED
