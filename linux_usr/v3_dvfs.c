#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "v3_user_dvfs.h"

void usage() 
{
    fprintf(stderr,"usage: v3_dvfs core|core..core command\n");
    fprintf(stderr,"    where command is one of the following\n");
    fprintf(stderr,"           acquire direct|external\n");
    fprintf(stderr,"           pstate  number\n");
    fprintf(stderr,"           freq    number (kHz)\n");
    fprintf(stderr,"           release\n\n"); 
    fprintf(stderr,"Look at /proc/v3vee/v3-dvfs to see state\n\n");
}

  

int main(int argc, char *argv[])
{
  if (argc!=4 && argc!=3) { 
    usage();
    return -1;
  }


  uint32_t corestart, coreend, core ;
  int rc;
  char *cmd=argv[2];

  if (strstr(argv[1],"..")) { 
      if (sscanf(argv[1],"%u..%u",&corestart,&coreend)!=2) {
	  usage();
	  return -1;
      }
  } else {
      corestart=coreend=atoi(argv[1]);
  }


  if (argc==3 && strcasecmp(cmd,"release")) { 
      usage();
      return -1;
  }

  char *arg=argv[3];

  rc=0;

  for (core=corestart;core<=coreend;core++) {
      if (!strcasecmp(cmd,"acquire")) { 
	  if (!strcasecmp(arg,"direct")) { 
	      if (v3_user_dvfs_acquire_direct(core)) { 
		  fprintf(stderr,"Failed to set core %u to direct\n",core);
		  rc=-1;
	      } else {
		  fprintf(stderr,"Core %u set to direct\n",core);
	      }
	  } else if (!strcasecmp(arg,"external")) {
	      if (v3_user_dvfs_acquire_external(core)) { 
		  fprintf(stderr,"Failed to set core %u to external\n",core);
		  rc=-1;
	      } else {
		  fprintf(stderr,"Core %u set to external\n",core);
	      }
	  } else {
	      usage();
	      return -1;
	  }
      } else if (!strcasecmp(cmd,"pstate")) { 
	  if (v3_user_dvfs_set_pstate(core,atoi(arg))) { 
	      fprintf(stderr,"Failed to set core %u to pstate %d\n",core,atoi(arg));
	      rc=-1;
	  } else {
	      fprintf(stderr,"Core %u set to pstate %d\n",core,atoi(arg));
	  }
      } else if (!strcasecmp(cmd,"freq")) { 
	  if (v3_user_dvfs_set_freq(core,atoll(arg))) { 
	      fprintf(stderr,"Failed to set core %u to frequency %lld kHz\n",core,atoll(arg));
	      rc=-1;
	  } else {
	      fprintf(stderr,"Core %u set to frequency %lld kHz\n",core,atoll(arg));
	  }
      } else if (!strcasecmp(cmd,"release")) { 
	  if (v3_user_dvfs_release(core)) { 
	      fprintf(stderr,"Failed to release core %u to host control\n",core);
	      rc=-1;
	  } else {
	      fprintf(stderr,"Released core %u to host control\n",core);
	  }
      } else {
	  usage();
	  return -1;
      }
  }

  return rc;

}
      
