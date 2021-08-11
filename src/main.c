/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include "sysio.h"
#include "module_loader.h"
#include "rela/convert.h"
#include "rela/core.h"
#include "rela/register.h"
#include "rela/data_register.h"
#include "rela/module_relocation_types.h"

const char *find_item(int argc, char *argv[], const char *name){

	for(int i=0;i<argc;i++){
		if(strstr(argv[i], name) != NULL){
			return (char *)(strstr(argv[i], name) + strlen(name));
		}
	}

	return NULL;
}

int main(int argc, char *argv[]){

	int res;
	ModuleLoaderContext *pContext;

	const char *src_path = find_item(argc, argv, "-src=");
	const char *dst_path = find_item(argc, argv, "-dst=");

	if(argc == 1 || src_path == NULL || dst_path == NULL){
		printf("psp2rela -src=in_file -dst=out_file\n");
		return 1;
	}

	res = module_loader_open(src_path, &pContext);
	if(res < 0){
		printf("cannot open module\n");
		return 1;
	}

	void *rel_config0 = NULL, *rel_config1 = NULL, *rel_config0_res = NULL, *rel_config1_res = NULL;
	long unsigned int rel_config_size0 = 0, rel_config_size1 = 0;
	int rel_config_size0_res = 0, rel_config_size1_res = 0;

	res = module_loader_search_elf_index(pContext, 0x60000000, 0);
	if(res >= 0){

		int idx = res;

		rel_config_size0 = pContext->pPhdr[idx].p_filesz;
		rel_config0 = malloc(rel_config_size0);

		if(pContext->pSegmentInfo[idx].compression == 2){
			res = uncompress(rel_config0, &rel_config_size0, pContext->segment[idx].pData, pContext->pSegmentInfo[idx].length);
			if(res != Z_OK){
				printf("zlib uncompress failed : 0x%X\n", res);
				goto error;
			}
		}else{
			memcpy(rel_config0, pContext->segment[idx].pData, pContext->pPhdr[idx].p_filesz);
		}
	}

	res = module_loader_search_elf_index(pContext, 0x60000000, 0x10000);
	if(res >= 0){

		int idx = res;

		rel_config_size1 = pContext->pPhdr[idx].p_filesz;
		rel_config1 = malloc(rel_config_size1);

		if(pContext->pSegmentInfo[idx].compression == 2){
			res = uncompress(rel_config1, &rel_config_size1, pContext->segment[idx].pData, pContext->pSegmentInfo[idx].length);
			if(res != Z_OK){
				printf("zlib uncompress failed : 0x%X\n", res);
				goto error;
			}
		}else{
			memcpy(rel_config1, pContext->segment[idx].pData, pContext->pPhdr[idx].p_filesz);
		}
	}

	/*
	 * Convert text segment rel config
	 */
	res = rela_regiser_entrys(rel_config0, rel_config_size0, 0);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", "text", 0);
		goto error;
	}

	res = rela_regiser_entrys(rel_config1, rel_config_size1, 0); // Register to split the merged config of vitasdk
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", "text", 1);
		goto error;
	}

	res = rela_data_sort_all();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_sort_all", "text", 0);
		goto error;
	}

	res = rela_data_register_open();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_open", "text", 0);
		goto error;
	}

	res = rela_data_convert(0);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_convert", "text", 0);
		goto error;
	}

	res = rela_data_register_close(&rel_config0_res, &rel_config_size0_res);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_close", "text", 0);
		goto error;
	}

	res = rela_data_free();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_free", "text", 0);
		goto error;
	}

	/*
	 * Convert data segment rel config
	 */
	res = rela_regiser_entrys(rel_config0, rel_config_size0, 1); // Register to split the merged config of vitasdk
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", "data", 0);
		goto error;
	}

	res = rela_regiser_entrys(rel_config1, rel_config_size1, 1);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_regiser_entrys", "data", 1);
		goto error;
	}

	res = rela_data_sort_all();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_sort_all", "data", 0);
		goto error;
	}

	res = rela_data_register_open();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_open", "data", 0);
		goto error;
	}

	res = rela_data_convert(1);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_convert", "data", 0);
		goto error;
	}

	res = rela_data_register_close(&rel_config1_res, &rel_config_size1_res);
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_register_close", "data", 0);
		goto error;
	}

	res = rela_data_free();
	if(res < 0){
		printf("%s failed in %s segment %d\n", "rela_data_free", "data", 0);
		goto error;
	}

	/*
	 * Clean up and settings
	 */
	free(rel_config0);
	rel_config0 = NULL;
	free(rel_config1);
	rel_config1 = NULL;

	rel_config0 = rel_config0_res;
	rel_config1 = rel_config1_res;
	rel_config_size0 = rel_config_size0_res;
	rel_config_size1 = rel_config_size1_res;
	rel_config0_res = NULL;
	rel_config1_res = NULL;

	/*
	 * Settings converted rel configs
	 */
	res = module_loader_search_elf_index(pContext, 0x60000000, 0);
	if(res < 0)
		res = module_loader_add_elf_entry(pContext, 0x60000000, 0);

	if(res >= 0){

		int idx = res;

		pContext->pPhdr[idx].p_filesz = rel_config_size0;

		if(pContext->pSegmentInfo[idx].compression == 2){

			long unsigned int rel_config_size0_tmp = rel_config_size0 << 1;
			void *rel_config0_tmp = malloc(rel_config_size0_tmp);

			compress(rel_config0_tmp, &rel_config_size0_tmp, rel_config0, rel_config_size0);
			free(rel_config0);
			rel_config0 = rel_config0_tmp;
			rel_config_size0 = rel_config_size0_tmp;
		}

		free(pContext->segment[idx].pData);
		pContext->segment[idx].pData = rel_config0;
		pContext->pSegmentInfo[idx].length = rel_config_size0;
	}else{
		free(rel_config0);
		rel_config0 = NULL;
	}

	res = module_loader_search_elf_index(pContext, 0x60000000, 0x10000);
	if(res < 0 && rel_config1 != NULL)
		res = module_loader_add_elf_entry(pContext, 0x60000000, 0x10000);

	if(res >= 0 && rel_config1 != NULL){

		int idx = res;

		pContext->pPhdr[idx].p_filesz = rel_config_size1;

		if(pContext->pSegmentInfo[idx].compression == 2){

			long unsigned int rel_config_size1_tmp = rel_config_size1 << 1;
			void *rel_config1_tmp = malloc(rel_config_size1_tmp);

			compress(rel_config1_tmp, &rel_config_size1_tmp, rel_config1, rel_config_size1);
			free(rel_config1);
			rel_config1 = rel_config1_tmp;
			rel_config_size1 = rel_config_size1_tmp;
		}

		free(pContext->segment[idx].pData);
		pContext->segment[idx].pData = rel_config1;
		pContext->pSegmentInfo[idx].length = rel_config_size1;
	}else{
		free(rel_config1);
		rel_config1 = NULL;
	}

	/*
	 * Save re converted module
	 */
	module_loader_save(pContext, dst_path);

module_close:
	module_loader_close(pContext);
	pContext = NULL;

	return 0;

error:
	if(rel_config0 != NULL){
		free(rel_config0);
		rel_config0 = NULL;
	}

	if(rel_config1 != NULL){
		free(rel_config1);
		rel_config1 = NULL;
	}

	if(rel_config0_res != NULL){
		free(rel_config0_res);
		rel_config0_res = NULL;
	}

	if(rel_config1_res != NULL){
		free(rel_config1_res);
		rel_config1_res = NULL;
	}

	module_loader_close(pContext);
	pContext = NULL;

	return 1;
}
