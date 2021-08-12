/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#ifndef _PSP2_RELA_CORE_H_
#define _PSP2_RELA_CORE_H_

typedef struct SceRelaTarget {
	struct SceRelaTarget *next;
	struct SceRelaTarget *prev; // for sort
	uint32_t symbol_segment; // for ABS32 rela
	uint32_t symbol_address; // for ABS32 rela
	uint32_t target_segment;
	uint32_t target_address;
	int type;
} SceRelaTarget;

typedef struct SceRelaData {
	struct SceRelaData *next;
	uint32_t symbol_segment;
	uint32_t symbol_address;
	SceRelaTarget *target_tree;
} SceRelaData;

int rela_data_free(void);
int rela_data_get_registered_num(void);
int rela_data_show(void);
int rela_data_sort_all(void);
int rela_data_search_by_symbol_address(uint32_t segment, uint32_t address, SceRelaData **ppRelaData);
int rela_data_split_abs32(uint32_t segment, SceRelaTarget **ppRelaTarget);
int rela_data_get_lowest_entry_by_target(uint32_t segment, uint32_t address, SceRelaData **ppRelaData);
int rela_data_add_entry(uint32_t target_segment, uint32_t offset_target, uint32_t symbol_segment, uint32_t offset_symbol, int type, uint32_t x_target_segment);

#endif /* _PSP2_RELA_CORE_H_ */
