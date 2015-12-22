#include <stdint.h>
#include <stdio.h>

#include "v3_guest_mem.h"

void usage() 
{
    fprintf(stderr,"usage: v3_guest_mem_track /dev/v3-vmN command\n");
    fprintf(stderr,"    where command is one of the following\n");
    fprintf(stderr,"           start oneshot|periodic period access\n");
    fprintf(stderr,"               access=combination of rwx (only rwx currently)\n");
    fprintf(stderr,"           stop\n");
    fprintf(stderr,"           snapshot bin|text file|_\n\n");
}

int start(char *vm, char *reset_str, char *period_str, char *access_str)
{
  v3_mem_track_access_t access;
  v3_mem_track_reset_t reset;
  uint64_t period;

  if (!strcasecmp(reset_str,"oneshot")) { 
    reset=V3_MEM_TRACK_ONESHOT;
  } else if (!strcasecmp(reset_str,"periodic")) { 
    reset=V3_MEM_TRACK_PERIODIC;
  } else {
    usage();
    return -1;
  }

  if (!strcasecmp(access_str,"rwx")) {
    access=V3_MEM_TRACK_ACCESS;
  } else {
    usage();
    return -1;
  }

  period=atoll(period_str);

  return v3_guest_mem_track_start(vm,access,reset,period);
}


int stop(char *vm)
{
  return v3_guest_mem_track_stop(vm);
}

int text_out(char *vm, char *target, v3_mem_track_snapshot *s)
{
  FILE *fd;
  int close=0;
  struct timeval tv;
  double thetime;
  uint64_t i,j;
  uint64_t count;

  gettimeofday(&tv,0);
  thetime = tv.tv_sec + ((double)tv.tv_usec)/1.0e6;

  if (!strcasecmp(target,"_")) { 
    fd = stdout;
  } else {
    if (!(fd=fopen(target,"w"))) { 
      fprintf(stderr,"Cannot open %s for write\n",target);
      return -1;
    }
    close=1;
  }

  fprintf(fd,"Memory Tracking Snapshot\n\n");
  fprintf(fd,"VM:\t%s\n",vm);
  fprintf(fd,"Time:\t%lf\n\n",thetime);
  fprintf(fd,"Reset:\t%s\n", 
	  s->reset_type==V3_MEM_TRACK_ONESHOT ? "oneshot" :
	  s->reset_type==V3_MEM_TRACK_PERIODIC ? "periodic" : "UNKNOWN");
  fprintf(fd,"Interval:\t%lu cycles\n", s->period);
  fprintf(fd,"Access:\t%s%s%s\n",
	  s->access_type&V3_MEM_TRACK_READ ? "r" : "",
	  s->access_type&V3_MEM_TRACK_WRITE ? "w" : "",
	  s->access_type&V3_MEM_TRACK_EXEC ? "x" : "");
	  
  fprintf(fd,"Cores:\t%u\n",s->num_cores);
  fprintf(fd,"Pages:\t%lu\n\n",s->core[0].num_pages);

  
  for (i=0;i<s->num_cores;i++) { 
    fprintf(fd,"Core %lu (%lu to %lu, ",
	    i, s->core[i].start_time, s->core[i].end_time);
    count=0;
    for (j=0;j<s->core[i].num_pages;j++) {
	count+=GET_BIT(s->core[i].access_bitmap,j);
    }
    fprintf(fd," %lu pages touched) : ", count);
    for (j=0;j<s->core[i].num_pages;j++) {
      if (GET_BIT(s->core[i].access_bitmap,j)) { 
	fprintf(fd,"X");
      } else {
	fprintf(fd,".");
      }
    }
    fprintf(fd,"\n");
  }

  fprintf(fd,"\n");

  if (close) { 
    fclose(fd);
  }
  
  return 0;

}
  
int write_all(int fd, void *buf, int n)
{
  int rc;

  while (n) { 
    rc=write(fd,buf,n);
    if (rc<=0) { 
      return -1;
    } 
    buf+=rc;
    n-=rc;
  }
  return 0;
}


int bin_out(char *vm, char *target, v3_mem_track_snapshot *s)
{
  int fd;
  int doclose=0;
  struct timeval tv;
  double thetime;
  uint64_t i;

  gettimeofday(&tv,0);
  thetime = tv.tv_sec + ((double)tv.tv_usec)/1.0e6;

  if (!strcasecmp(target,"_")) { 
    fd = fileno(stdout);
  } else {
    if ((fd=open(target,O_WRONLY | O_CREAT, 0600))<0) { 
      fprintf(stderr,"Cannot open %s for write\n",target);
      return -1;
    }
    doclose=1;
  }

#define WRITE(x) write_all(fd, &(x), sizeof(x))

  WRITE(thetime);
  WRITE(s->reset_type);
  WRITE(s->period);
  WRITE(s->access_type);
  WRITE(s->num_cores);
  WRITE(s->core[0].num_pages);
  for (i=0;i<s->num_cores;i++) { 
    WRITE(i);
    WRITE(s->core[i].start_time);
    WRITE(s->core[i].end_time);
    write_all(fd,s->core[i].access_bitmap,s->core[i].num_pages/8 + !!(s->core[i].num_pages%8));
  }

  if (doclose) { 
    close(fd);
  }
  
  return 0;

}

int snapshot(char *vm, char *format, char *target)
{
  v3_mem_track_snapshot *snap;

  if (!(snap=v3_guest_mem_track_snapshot(vm))) { 
    return -1;
  }

  if (!strcasecmp(format,"bin")) { 
    int rc = bin_out(vm,target, snap);
    v3_guest_mem_track_free_snapshot(snap);
    return rc;
  } else if (!strcasecmp(format,"text")) { 
    int rc = text_out(vm,target, snap);
    v3_guest_mem_track_free_snapshot(snap);
    return rc;
  } else {
    return -1;
  }
}
  

int main(int argc, char *argv[])
{
  char *vm=argv[1];
  char *cmd=argv[2];

  if (argc<3) { 
    usage();
    return -1;
  }

  if (!strcasecmp(cmd,"start")) { 
    if (argc!=6) { 
      usage();
      return -1;
    } else {
      return start(vm,argv[3],argv[4],argv[5]);
    }
  } else if (!strcasecmp(cmd,"stop")) { 
    if (argc!=3) { 
      usage();
      return -1;
    } else {
      return stop(vm);
    } 
  } else if (!strcasecmp(cmd,"snapshot")) { 
    if (argc!=5) { 
      usage();
      return -1;
    } else {
      return snapshot(vm,argv[3],argv[4]);
    }
  } else {
    usage();
    return -1;
  }

  return 0;
}
  
