/*
 * GeekOS timer interrupt support
 * Copyright (c) 2001, David H. Hovemeyer <daveho@cs.umd.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * $Revision: 1.2 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_TIMER_H
#define GEEKOS_TIMER_H

#define TIMER_IRQ 0

extern volatile unsigned long g_numTicks;

typedef void (*timerCallback)(int, void*);

void Init_Timer(void);

void Micro_Delay(int us);


typedef struct {
  int ticks;                           /* timer code decrements this */
  int id;                              /* unqiue id for this timer even */
  timerCallback callBack;              /* Queue to wakeup on timer expire */
  void * cb_arg;                       /* Argument to add to callback */
  int origTicks;

} timerEvent;

int Start_Timer_Secs(int seconds, timerCallback cb, void * arg);
int Start_Timer_MSecs(int msecs, timerCallback cb, void * arg);
int Start_Timer(int ticks, timerCallback, void * arg);


double Get_Remaining_Timer_Secs(int id);
int Get_Remaining_Timer_MSecs(int id);
int Get_Remaining_Timer_Ticks(int id);
int Cancel_Timer(int id);

void Micro_Delay(int us);

unsigned long clock_time(void);  //return elipsed millisecs

#endif  /* GEEKOS_TIMER_H */
