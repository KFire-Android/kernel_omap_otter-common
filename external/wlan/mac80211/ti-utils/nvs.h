#ifndef __NVS_H
#define __NVS_H

#define WL127X_NVS_FILE_SZ		912
#define WL128X_NVS_FILE_SZ		1113

char *get_opt_nvsinfile(int argc, char **argv);
char *get_opt_nvsoutfile(int argc, char **argv);

int prepare_nvs_file(void *arg, char *file_name);

int nvs_set_mac(char *nvsfile, char *mac);

void cfg_nvs_ops(struct wl12xx_common *cmn);

int create_nvs_file(struct wl12xx_common *cmn);

int update_nvs_file(const char *nvs_infile, const char *nvs_outfile,
	struct wl12xx_common *cmn);

int dump_nvs_file(const char *nvs_file);

int set_nvs_file_autofem(const char *nvs_file, unsigned char val,
	struct wl12xx_common *cmn);

int set_nvs_file_fem_manuf(const char *nvs_file, unsigned char val,
	struct wl12xx_common *cmn);

int info_nvs_file(const char *nvs_file);

#endif /* __NVS_H */
