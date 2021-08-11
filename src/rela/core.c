/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "module_relocation_types.h"

SceRelaData *pRelaDataTop = NULL;

int rela_data_free(void){

	SceRelaData *pRelaData = pRelaDataTop;
	while(pRelaData != NULL){

		SceRelaData *pRelaDataNext = pRelaData->next;

		SceRelaTarget *target_tree = pRelaData->target_tree;
		while(target_tree != NULL){
			SceRelaTarget *target_tree_next = target_tree->next;
			free(target_tree);
			target_tree = target_tree_next;
		}

		free(pRelaData);
		pRelaData = pRelaDataNext;
	}

	pRelaDataTop = NULL;

	return 0;
}

int rela_data_show(void){

	SceRelaData *pRelaData = pRelaDataTop;

	while(pRelaData != NULL){
		printf("symbol=%d:0x%08X\n", pRelaData->symbol_segment, pRelaData->symbol_address);

		SceRelaTarget *target_tree = pRelaData->target_tree;
		while(target_tree != NULL){
			printf("\t%d:0x%08X type=0x%X\n", target_tree->target_segment, target_tree->target_address, target_tree->type);
			target_tree = target_tree->next;
		}

		pRelaData = pRelaData->next;
	}

	return 0;
}

int rela_data_search_by_symbol_address(uint32_t segment, uint32_t address, SceRelaData **ppRelaData){

	SceRelaData *pRelaData = pRelaDataTop;

	while(pRelaData != NULL){

		if(pRelaData->symbol_segment == segment && pRelaData->symbol_address == address){
			if(ppRelaData != NULL)
				*ppRelaData = pRelaData;

			return 0;
		}

		pRelaData = pRelaData->next;
	}

	return -1;
}

int rela_data_sort_target_by_target_address(SceRelaTarget **ppRelaTarget, int enable_out_rollback){

	SceRelaTarget *pRelaTarget, *pRelaTargetRestorePoint;

	if(ppRelaTarget == NULL){
		return -1;
	}

	pRelaTargetRestorePoint = NULL;
	pRelaTarget = *ppRelaTarget;
	while(pRelaTarget->next != NULL){
		if(pRelaTarget->next->target_address < pRelaTarget->target_address){

			SceRelaTarget tmp;
			memcpy(&tmp, pRelaTarget, sizeof(tmp));

			pRelaTarget->symbol_segment = pRelaTarget->next->symbol_segment;
			pRelaTarget->symbol_address = pRelaTarget->next->symbol_address;
			pRelaTarget->target_segment = pRelaTarget->next->target_segment;
			pRelaTarget->target_address = pRelaTarget->next->target_address;
			pRelaTarget->type           = pRelaTarget->next->type;

			pRelaTarget->next->symbol_segment = tmp.symbol_segment;
			pRelaTarget->next->symbol_address = tmp.symbol_address;
			pRelaTarget->next->target_segment = tmp.target_segment;
			pRelaTarget->next->target_address = tmp.target_address;
			pRelaTarget->next->type           = tmp.type;

			if(pRelaTargetRestorePoint == NULL)
				pRelaTargetRestorePoint = pRelaTarget;

			if(pRelaTarget->prev != NULL)
				pRelaTarget = pRelaTarget->prev;

		}else{
			if(pRelaTargetRestorePoint != NULL){
				pRelaTarget = pRelaTargetRestorePoint;
				pRelaTargetRestorePoint = NULL;
			}else{
				pRelaTarget = pRelaTarget->next;
			}
		}
	}

	if(enable_out_rollback != 0){
		while(pRelaTarget->prev != NULL){
			pRelaTarget = pRelaTarget->prev;
		}
	}

	*ppRelaTarget = pRelaTarget;

	return 0;
}


#define SCE_RELA_ABS32_SORT_ENABLE (1)

int rela_data_split_abs32(uint32_t segment, SceRelaTarget **ppRelaTarget){

	SceRelaTarget *target_tree_current = NULL, *target_symbol_segment1 = NULL, *new_target, *tmp_target, *target_next;
	SceRelaData *pRelaData = pRelaDataTop;

	if(ppRelaTarget == NULL)
		return -1;

	*ppRelaTarget = NULL;

	while(pRelaData != NULL){

		SceRelaTarget *target_tree = pRelaData->target_tree;
		while(target_tree != NULL){

			// printf("target=%d:0x%08X symbol=%d:0x%08X type=0x%02X\n", target_tree->target_segment, target_tree->target_address, target_tree->symbol_segment, target_tree->symbol_address, target_tree->type);

			if(target_tree->target_segment == segment && (target_tree->type == R_ARM_ABS32 || target_tree->type == R_ARM_TARGET1)){

				if(target_tree == pRelaData->target_tree)
					pRelaData->target_tree = target_tree->next;

				// unlink entry
				if(target_tree->prev != NULL)
					target_tree->prev->next = target_tree->next;

				if(target_tree->next != NULL)
					target_tree->next->prev = target_tree->prev;

				// add entry to ABS32 list
				target_next = target_tree->next;

				target_tree->next           = NULL;

				if(pRelaData->symbol_segment == 1){
					target_tree->prev           = target_symbol_segment1;
				}else{
					target_tree->prev           = target_tree_current;
				}
				target_tree->symbol_segment = pRelaData->symbol_segment;
				target_tree->symbol_address = pRelaData->symbol_address;

				if(pRelaData->symbol_segment == 1){
					if(target_symbol_segment1 != NULL)
						target_symbol_segment1->next  = target_tree;
					target_symbol_segment1        = target_tree;
				}else{
					if(target_tree_current != NULL)
						target_tree_current->next  = target_tree;
					target_tree_current        = target_tree;
				}
				target_tree = target_next;
			}else{
				target_tree = target_tree->next;
			}
		}

		pRelaData = pRelaData->next;
	}

#if defined(SCE_RELA_ABS32_SORT_ENABLE) && SCE_RELA_ABS32_SORT_ENABLE != 0

	if(target_tree_current == NULL){
		target_tree_current = target_symbol_segment1;
		target_symbol_segment1 = NULL;
	}

	if(target_tree_current == NULL)
		goto end;

	while(target_tree_current->prev != NULL){
		target_tree_current = target_tree_current->prev;
	}

	rela_data_sort_target_by_target_address(&target_tree_current, 0);

	if(target_symbol_segment1 != NULL){
		while(target_symbol_segment1->prev != NULL){
			target_symbol_segment1 = target_symbol_segment1->prev;
		}

		rela_data_sort_target_by_target_address(&target_symbol_segment1, 1);
	}

	if(target_tree_current->next != NULL){
		printf("target_tree_current->next != NULL\n");
	}

	target_tree_current->next = target_symbol_segment1;

#endif

	while(target_tree_current->prev != NULL){
		target_tree_current = target_tree_current->prev;
	}

end:
	*ppRelaTarget = target_tree_current;

	return 0;
}

int rela_data_get_lowest_entry_by_target(uint32_t segment, uint32_t address, SceRelaData **ppRelaData){

	uint32_t cache_address = ~0;
	SceRelaData *pRelaData = pRelaDataTop;

	if(ppRelaData == NULL)
		return -1;

	*ppRelaData = NULL;

	while(pRelaData != NULL){
		if(pRelaData->target_tree != NULL && pRelaData->target_tree->target_segment == segment){
			if(address < pRelaData->target_tree->target_address && cache_address > pRelaData->target_tree->target_address){
				cache_address = pRelaData->target_tree->target_address;
				*ppRelaData = pRelaData;
			}
		}

		pRelaData = pRelaData->next;
	}

	return 0;
}

int rela_data_sort_all(void){

	SceRelaData *pRelaData = pRelaDataTop;

	while(pRelaData != NULL){
		rela_data_sort_target_by_target_address(&pRelaData->target_tree, 1);
		pRelaData = pRelaData->next;
	}

	return 0;
}

int rela_data_add_entry(
	uint32_t target_segment, uint32_t offset_target,
	uint32_t symbol_segment, uint32_t offset_symbol,
	int type, uint32_t x_target_segment
	){

	int res;
	SceRelaData *pRelaData = NULL;

	if(target_segment != x_target_segment || type == R_ARM_NONE){
		if(0){
			printf(
				"Skip : symbol=%d:0x%08X target=%d:0x%08X type=0x%X\n",
				symbol_segment, offset_symbol,
				target_segment, offset_target,
				type
			);
		}
		return 0;
	}

retry:
	res = rela_data_search_by_symbol_address(symbol_segment, offset_symbol, &pRelaData);
	if(res >= 0){

		SceRelaTarget *rela_target = pRelaData->target_tree;

		rela_target = malloc(sizeof(*rela_target));
		memset(rela_target, 0, sizeof(*rela_target));

		rela_target->next = pRelaData->target_tree;
		rela_target->prev = NULL;
		rela_target->symbol_segment = symbol_segment;
		rela_target->symbol_address = offset_symbol;
		rela_target->target_segment = target_segment;
		rela_target->target_address = offset_target;
		rela_target->type = type;

		if(pRelaData->target_tree != NULL)
			pRelaData->target_tree->prev = rela_target;

		pRelaData->target_tree = rela_target;

	}else{
		pRelaData = malloc(sizeof(*pRelaData));
		memset(pRelaData, 0, sizeof(*pRelaData));

		pRelaData->next = pRelaDataTop;
		pRelaData->symbol_segment = symbol_segment;
		pRelaData->symbol_address = offset_symbol;
		pRelaData->target_tree = NULL;

		pRelaDataTop = pRelaData;
		pRelaData = NULL;


		if(0){
			printf("register symbol=%d:0x%08X\n", symbol_segment, offset_symbol);
		}


		if(0){
			printf(
				"symbol=%d:0x%08X target=%d:0x%08X type=0x%X\n",
				symbol_segment, offset_symbol,
				target_segment, offset_target,
				type
			);
		}

		goto retry;
	}

	return 0;
}
