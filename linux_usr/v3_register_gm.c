/* 
 * V3 Guarded Module registration utility
 *
 * This code allows a user to register a 
 * guest driver module to be guarded upon
 * injection into a guest.  *
 * (c) Kyle C. Hale, 2012
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "v3_ctrl.h"
#include "iface-guard-mods.h"
#include "cJSON.h"

#define SET_PRIV(x, i) ((x) |= 1U << (i))


/* Parse text to JSON, then render back to text, and print! */
static void 
populate_gm (char * filename, struct v3_guard_mod * m)
{
    int fd, i, nents, nrets;
    char * data;
    struct stat stats;
    cJSON *json, *tmp, *tmp2, *ep, *sub_ep;
    
	fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(0);
    }

    if (fstat(fd, &stats) == -1) {
        fprintf(stderr, "Error stating file: %s\n", filename);
        exit(0);
    }

	data = malloc(stats.st_size);
    v3_read_file(fd, stats.st_size, data);
    close(fd);

	json = cJSON_Parse(data);

	if (!json) {
        fprintf(stderr, "Error before: [%s]\n",cJSON_GetErrorPtr());
        goto out;
    } 

    m->name = cJSON_Print(cJSON_GetObjectItem(json, "module_name"));
    m->content_hash = cJSON_Print(cJSON_GetObjectItem(json, "content_hash"));

    tmp = cJSON_GetObjectItem(json, "size");
    m->text_size = tmp->valueint;

    tmp = cJSON_GetObjectItem(json, "hcall_offset");
    m->hcall_offset = tmp->valueint;

    /* extract all the valid entry points */
    tmp = cJSON_GetObjectItem(json, "entry_points");
    nents = cJSON_GetArraySize(tmp);

    tmp2 = cJSON_GetObjectItem(json, "ret_points");
    nrets = cJSON_GetArraySize(tmp2);


    m->num_entries = nents + nrets;
    printf("num entries: %d, nents: %d, nrets: %d\n", m->num_entries, nents, nrets);
    m->entry_points = malloc(sizeof(struct v3_entry_point)*m->num_entries);

    for (i = 0; i < nents; i++) {
        ep = cJSON_GetArrayItem(tmp, i);
        sub_ep = cJSON_GetArrayItem(ep, 0);

        m->entry_points[i].name = cJSON_Print(sub_ep);
        m->entry_points[i].is_ret = 0;

        sub_ep = cJSON_GetArrayItem(ep, 1);
        m->entry_points[i].offset = sub_ep->valueint;
    }


    for (i = nents; i < m->num_entries; i++) {
        ep = cJSON_GetArrayItem(tmp2, i - nents);
        sub_ep = cJSON_GetArrayItem(ep, 0);

        m->entry_points[i].name = cJSON_Print(sub_ep);
        m->entry_points[i].is_ret = 1;

        sub_ep = cJSON_GetArrayItem(ep, 1);
        m->entry_points[i].offset = sub_ep->valueint;
    }

    tmp = cJSON_GetObjectItem(json, "privileges");
    m->num_privs = cJSON_GetArraySize(tmp);
    m->priv_array = malloc(sizeof(char*)*m->num_privs);
    if (!m->priv_array) {
        fprintf(stderr, "Problem allocating privilege array in userspace\n");
        goto out;
    }

    for (i = 0; i < m->num_privs; i++) {
        ep = cJSON_GetArrayItem(tmp, i);
        m->priv_array[i] = cJSON_Print(ep);
    }

out:
    cJSON_Delete(json);
	free(data);
}


int main (int argc, char **argv) {
    struct v3_guard_mod mod; 
    char *dev_file, *json_file;
    uint64_t ret;

    if (argc < 2) {
        v3_usage("<vm-device> <json>\n");
        return -1;
    }

    dev_file  = argv[1];
    json_file = argv[2];

    populate_gm(json_file, &mod);

    printf("Registering guarded module: %s, size: %d, offset: %d\n", mod.name, mod.text_size, mod.hcall_offset);

    mod.id = 0;

    ret = v3_vm_ioctl(dev_file, V3_VM_REGISTER_MOD, &mod);

    if (ret < 0) {
        fprintf(stderr, "Problem registering module\n");
        return -1;
    }
    
    if (!mod.id) {
        fprintf(stderr, "Could not register guarded module\n");
    } else {
        printf("Module successfully registered [0x%llx]\n", mod.id);
    }

	return 0;
}
