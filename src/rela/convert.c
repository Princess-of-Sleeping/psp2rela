/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdint.h>
#include <stdlib.h>
#include "convert.h"
#include "core.h"
#include "register.h"
#include "data_register.h"

// WIP
#define SCE_RELA_USE_ABS32_TYPE (1)

#define printf(...) do{}while(0)

/*
 * Used by ABS32
 */
#define RELA_TYPE9_MAX_WORDS (0xE)
#define RELA_TYPE8_MAX_WORDS (0x7)
#define RELA_TYPE7_MAX_WORDS (0x4)
#define RELA_TYPE9_BIT_RANGE ((32 - 4) / RELA_TYPE9_MAX_WORDS)
#define RELA_TYPE8_BIT_RANGE ((32 - 4) / RELA_TYPE8_MAX_WORDS)
#define RELA_TYPE7_BIT_RANGE ((32 - 4) / RELA_TYPE7_MAX_WORDS)

const int type789_max_words_list[3] = {RELA_TYPE9_MAX_WORDS, RELA_TYPE8_MAX_WORDS, RELA_TYPE7_MAX_WORDS};
const int type789_bit_range_list[3] = {RELA_TYPE9_BIT_RANGE, RELA_TYPE8_BIT_RANGE, RELA_TYPE7_BIT_RANGE};

int rela_data_convert(uint32_t segment){

	int res, use_first_type;
	uint32_t address = 0, append_offset1, append_offset2, rel_type0, rel_type1;
	uint32_t symbol_segment, current_symbol, target_segment, current_target;

	SceRelaData *pRelaData;
	SceRelaTarget *target_tree;

#if defined(SCE_RELA_USE_ABS32_TYPE) && SCE_RELA_USE_ABS32_TYPE != 0
	SceRelaTarget *pRelaTargetAbs32;
	rela_data_split_abs32(segment, &pRelaTargetAbs32);
#endif

	do {
		res = rela_data_get_lowest_entry_by_target(segment, address, &pRelaData);
		if(res < 0){
			printf("%s: %s failed = 0x%X\n", __FUNCTION__, "rela_data_get_lowest_entry_by_target", res);
			return res;
		}

		if(pRelaData != NULL){
			target_tree = pRelaData->target_tree;

			if(target_tree != NULL){
				address = target_tree->target_address; // for rela_data_get_lowest_entry_by_target

				symbol_segment = pRelaData->symbol_segment;
				current_symbol = pRelaData->symbol_address;
				target_segment = pRelaData->target_tree->target_segment;
				current_target = pRelaData->target_tree->target_address;
			}

			append_offset1 = 0;
			append_offset2 = 0;
			rel_type0 = R_ARM_NONE;
			rel_type1 = R_ARM_NONE;

			use_first_type = 1;

			SceRelaTarget *target0, *target1, *target2, *target3;

			while(target_tree != NULL){

				target0 = target_tree;
				target1 = target_tree->next;
				target2 = NULL;
				target3 = NULL;

				if(use_first_type != 0){
					use_first_type = 0;

					current_target = target0->target_address;
					target_segment = target0->target_segment;

					if(target1 != NULL){
						rel_type0 = target0->type;
						rel_type1 = target1->type;
						append_offset1 = target1->target_address - current_target;
					}else{
rel_first_type_none1:
						rel_type0 = target0->type;
						rel_type1 = R_ARM_NONE;
						append_offset1 = 0;
					}

					if(append_offset1 >= (1 << 5))
						goto rel_first_type_none1;

					if(rel_type1 != R_ARM_NONE || (current_target >= (1 << 22) || current_symbol >= (1 << 22))){
						res = rela_data_register_write_type0(
							symbol_segment, current_symbol,
							target_segment, current_target,
							append_offset1, rel_type0, rel_type1
						);

						printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 0, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);
					}else{
						res = rela_data_register_write_type1(
							symbol_segment, current_symbol,
							target_segment, current_target,
							rel_type0
						);

						printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 1, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);
					}

					if(res != 0){
						printf("Error happened in write first type : 0x%X\n", res);
						printf("Should be not happened error here (L%d)\n", __LINE__);
						printf(
							"symbol=%d:0x%08X target=%d:0x%08X append=0x%08X type=0x%02X/0x%02X\n",
							symbol_segment, current_symbol,
							target_segment, current_target,
							append_offset1, rel_type0, rel_type1
						);
						return res;
					}

					target_tree = (rel_type1 != R_ARM_NONE) ? target1->next : target0->next;
					continue;
				}

				if(target1 != NULL)
					target2 = target1->next;

				if(target2 != NULL)
					target3 = target2->next;

				if(target1 != NULL && target2 != NULL && target3 != NULL){
					if(
						((rel_type0 == R_ARM_THM_MOVW_ABS_NC && rel_type1 == R_ARM_THM_MOVT_ABS) ||
						(rel_type0 == R_ARM_MOVW_ABS_NC && rel_type1 == R_ARM_MOVT_ABS)) &&
						target0->type == rel_type0 &&
						target1->type == rel_type1 &&
						target2->type == rel_type0 &&
						target3->type == rel_type1 &&
						symbol_segment == target0->symbol_segment &&
						symbol_segment == target1->symbol_segment &&
						symbol_segment == target2->symbol_segment &&
						symbol_segment == target3->symbol_segment &&
						current_symbol == target0->symbol_address &&
						current_symbol == target1->symbol_address &&
						current_symbol == target2->symbol_address &&
						current_symbol == target3->symbol_address
					){
						append_offset1 = target1->target_address - target0->target_address;
						append_offset2 = target3->target_address - target2->target_address;

						res = rela_data_register_write_type5(
							target0->target_address - current_target, append_offset1,
							target2->target_address - target0->target_address, append_offset2
						);

						if(res == 0){
							printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 5, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);

							current_target = target2->target_address;
							target_tree = target3->next;
							continue;
						}
					}
				}

				if(target1 != NULL){
					if(
						((rel_type0 == R_ARM_THM_MOVW_ABS_NC && rel_type1 == R_ARM_THM_MOVT_ABS) ||
						(rel_type0 == R_ARM_MOVW_ABS_NC && rel_type1 == R_ARM_MOVT_ABS)) &&
						target0->type == rel_type0 &&
						target1->type == rel_type1 &&
						symbol_segment == target0->symbol_segment
					){
						append_offset1 = target1->target_address - target0->target_address;

						res = rela_data_register_write_type4(
							target0->target_address - current_target, append_offset1
						);

						if(res == 0){
							printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 4, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);

							current_target = target0->target_address;
							target_tree = target1->next;
							continue;
						}
					}

					if(
						((target0->type == R_ARM_THM_MOVW_ABS_NC && target1->type == R_ARM_THM_MOVT_ABS) ||
						(target0->type == R_ARM_MOVW_ABS_NC && target1->type == R_ARM_MOVT_ABS)) &&
						target0->symbol_segment == target1->symbol_segment
					){
						int _symbol_segment = symbol_segment, _rel_type0 = rel_type0, _rel_type1 = rel_type1;
						symbol_segment = target0->symbol_segment;

						if(target0->type == R_ARM_THM_MOVW_ABS_NC){
							rel_type0 = R_ARM_THM_MOVW_ABS_NC;
							rel_type1 = R_ARM_THM_MOVT_ABS;
						}else{
							rel_type0 = R_ARM_MOVW_ABS_NC;
							rel_type1 = R_ARM_MOVT_ABS;
						}

						append_offset1 = target1->target_address - target0->target_address;

						res = rela_data_register_write_type3(
							symbol_segment, current_symbol,
							target0->target_address - current_target, append_offset1,
							target0->type == R_ARM_THM_MOVW_ABS_NC
						);

						if(res == 0){
							printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 3, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);

							current_target = target0->target_address;
							target_tree = target1->next;
							continue;
						}

						// Restore for failback
						symbol_segment = _symbol_segment;
						rel_type0 = _rel_type0;
						rel_type1 = _rel_type1;
					}

					{
						int _symbol_segment = symbol_segment, _rel_type0 = rel_type0, _rel_type1 = rel_type1;
						symbol_segment = target0->symbol_segment;

						rel_type0 = target0->type;
						rel_type1 = R_ARM_NONE;

						res = rela_data_register_write_type2(
							symbol_segment, current_symbol,
							target0->target_address - current_target, rel_type0
						);

						if(res == 0){
							printf("type%d target=%d:0x%08X symbol=%d:0x%08X type=0x%02X/0x%02X\n", 2, target_segment, target0->target_address, symbol_segment, current_symbol, rel_type0, rel_type1);

							current_target = target0->target_address;
							target_tree = target1;
							continue;
						}

						// Restore for failback
						symbol_segment = _symbol_segment;
						rel_type0 = _rel_type0;
						rel_type1 = _rel_type1;
					}
				}

				use_first_type = 1;
				target_tree = target1;
			}
		}
	} while(pRelaData != NULL);

#if defined(SCE_RELA_USE_ABS32_TYPE) && SCE_RELA_USE_ABS32_TYPE != 0

	SceRelaTarget *pRelaTargetAbs32Next;
	uint32_t abs32_symbol_segment, abs32_symbol_address, abs32_target_segment, abs32_target_address, cache_address;

#define NEXT_ABS32 do {                                \
	pRelaTargetAbs32Next = pRelaTargetAbs32->next; \
	if(pRelaTargetAbs32Next != NULL)               \
		pRelaTargetAbs32Next->prev = NULL;     \
	free(pRelaTargetAbs32);                        \
	pRelaTargetAbs32 = pRelaTargetAbs32Next;       \
	}while(0)

	use_first_type = 1;

	while(pRelaTargetAbs32 != NULL){
		if(use_first_type != 0 || abs32_target_segment != pRelaTargetAbs32->target_segment){
			use_first_type = 0;

			abs32_symbol_segment = pRelaTargetAbs32->symbol_segment;
			abs32_symbol_address = pRelaTargetAbs32->symbol_address;
			abs32_target_segment = pRelaTargetAbs32->target_segment;
			cache_address        = pRelaTargetAbs32->target_address;

			res = rela_data_register_write_type1(abs32_symbol_segment, abs32_symbol_address, abs32_target_segment, cache_address, R_ARM_ABS32);
			if(res < 0)
				res = rela_data_register_write_type0(abs32_symbol_segment, abs32_symbol_address, abs32_target_segment, cache_address, 0, R_ARM_ABS32, R_ARM_NONE);

			printf("ASBS32 %d terget=%d:0x%08X, symbol=%d:0x%08X\n", 0, pRelaTargetAbs32->target_segment, cache_address, pRelaTargetAbs32->symbol_segment, pRelaTargetAbs32->symbol_address);

			if(res < 0){
				printf("ABS32 error\n");
				return res;
			}

			NEXT_ABS32;

		}else if(pRelaTargetAbs32->target_address > cache_address){

			uint32_t address_offset, shift_val, type789_offset = 0, abs32_found_count, counter = 0;

			address_offset = pRelaTargetAbs32->target_address - cache_address;

			while(type789_offset == 0 && counter < 3 && pRelaTargetAbs32 != NULL){

				abs32_found_count = 0;
				shift_val = 0;

				while(
					pRelaTargetAbs32 != NULL && abs32_found_count < type789_max_words_list[counter] &&
					pRelaTargetAbs32->target_address > cache_address &&
					address_offset < ((1 << type789_bit_range_list[counter]) * sizeof(uint32_t))
				){
					type789_offset |= ((address_offset / sizeof(uint32_t)) << shift_val);
					abs32_found_count++; shift_val += type789_bit_range_list[counter];

					cache_address = pRelaTargetAbs32->target_address;

					printf("ASBS32 %d target_offset=0x%08X (terget=%d:0x%08X, symbol=%d:0x%08X)\n", 9 - counter, address_offset, pRelaTargetAbs32->target_segment, cache_address, pRelaTargetAbs32->symbol_segment, pRelaTargetAbs32->symbol_address);

					NEXT_ABS32;
					if(pRelaTargetAbs32 != NULL)
						address_offset = pRelaTargetAbs32->target_address - cache_address;
				}

				counter++;
			}

			counter--;
			if(type789_offset != 0 && counter < 3){
				printf("type%d = 0x%08X\n", 9 - counter, type789_offset);
				if(counter == 0){
					rela_data_register_write_type9(type789_offset);
				}else if(counter == 1){
					rela_data_register_write_type8(type789_offset);
				}else if(counter == 2){
					rela_data_register_write_type7(type789_offset);
				}
			}else{
				if(pRelaTargetAbs32 != NULL && pRelaTargetAbs32->target_address > cache_address){
					res = rela_data_register_write_type6(pRelaTargetAbs32->target_address - cache_address);
					if(res >= 0){

						printf("ASBS32 %d target_offset=0x%08X (terget=%d:0x%08X, symbol=%d:0x%08X)\n", 6, pRelaTargetAbs32->target_address - cache_address, pRelaTargetAbs32->target_segment, pRelaTargetAbs32->target_address, pRelaTargetAbs32->symbol_segment, pRelaTargetAbs32->symbol_address);

						cache_address = pRelaTargetAbs32->target_address;
						NEXT_ABS32;
					}
					use_first_type = res < 0;
				}else{
					use_first_type = 1;
				}
			}
		}else{
			use_first_type = 1;
		}
	}

#undef printf
#endif

	return 0;
}
