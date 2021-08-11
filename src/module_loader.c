﻿/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sysio.h"
#include "module_loader.h"

int module_loader_close(ModuleLoaderContext *pContext){

	if(pContext == NULL){
		return -1;
	}

	for(int i=0;i<5;i++){
		if(pContext->segment[i].pData != NULL)
			free(pContext->segment[i].pData);
	}

	free(pContext->pSegmentInfo);
	free(pContext->pAppInfo);
	free(pContext->pVersion);
	free(pContext->pControlInfo);
	free(pContext->pPhdr);
	free(pContext->pHeader);
	free(pContext);

	return 0;
}

int module_loader_search_elf_index(ModuleLoaderContext *pContext, int type, int flags){

	if(pContext == NULL){
		return -1;
	}

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){
		if(pContext->pPhdr[i].p_type == type && pContext->pPhdr[i].p_flags == flags)
			return i;
	}

	return -2;
}

int module_loader_add_elf_entry(ModuleLoaderContext *pContext, int type, int flags){

	int res;

	if(pContext == NULL)
		return -1;

	if(pContext->pEhdr->e_phnum == 5)
		return -3;

	res = module_loader_search_elf_index(pContext, type, flags);
	if(res >= 0)
		return -4;

	int i = pContext->pEhdr->e_phnum;

	Elf32_Phdr *pPhdrTmp = (Elf32_Phdr *)malloc(((sizeof(Elf32_Phdr) * (i + 1)) + 0xF) & ~0xF);

	memset(pPhdrTmp, 0, ((sizeof(Elf32_Phdr) * (i + 1)) + 0xF) & ~0xF);
	memcpy(pPhdrTmp, pContext->pPhdr, sizeof(Elf32_Phdr) * i);
	free(pContext->pPhdr);
	pContext->pPhdr = pPhdrTmp;

	pContext->pPhdr[i].p_type  = type;
	pContext->pPhdr[i].p_flags = flags;
	pContext->pPhdr[i].p_align = 0x10;

	segment_info *pSegmentInfoTmp = (segment_info *)malloc(sizeof(segment_info) * (i + 1));
	memset(pSegmentInfoTmp, 0, sizeof(segment_info) * (i + 1));
	memcpy(pSegmentInfoTmp, pContext->pSegmentInfo, sizeof(segment_info) * i);
	free(pContext->pSegmentInfo);

	pContext->pSegmentInfo = pSegmentInfoTmp;
	pSegmentInfoTmp[i].offset = 0;
	pSegmentInfoTmp[i].length = 0;
	pSegmentInfoTmp[i].compression = 2; // 1 = uncompressed, 2 = compressed
	pSegmentInfoTmp[i].encryption  = 2; // 1 = encrypted,    2 = plain

	pContext->pEhdr->e_phnum = i + 1;

	return i;
}

int module_loader_open(const char *path, ModuleLoaderContext **ppResult){

	int fd, readbyte;
	void         *pHeader      = NULL;
	SCE_appinfo  *pAppInfo     = NULL;
	Elf32_Phdr   *pPhdr        = NULL;
	segment_info *pSegmentInfo = NULL;
	SCE_version  *pVersion     = NULL;
	void         *pControlInfo = NULL;
	ModuleLoaderContext context;

	if(ppResult == NULL)
		return -1;

	*ppResult = NULL;
	memset(&context, 0, sizeof(context));
	fd = -1;

	cf_header_t cf_header;
	memset(&cf_header, 0, sizeof(cf_header));

	fd = open(path, O_RDONLY | O_BINARY);
	if(fd < 0){
		printf("self open failed\n");
		return fd;
	}

	readbyte = read(fd, &cf_header, sizeof(cf_header));

	if(readbyte != sizeof(cf_header) || cf_header.base.m_magic != 0x454353){
		printf("This self is not SCE self\n");
		goto error;
	}

	if(cf_header.base.m_version != 3){
		printf("This self is not version 3\n");
		goto error;
	}

	if(cf_header.header_v3.m_certified_file_size > 0x10000000){
		printf("This self is too big\n");
		printf("Can't processing\n");
		goto error;
	}

	if(cf_header.header_v3.m_header_length > 0x1000){
		printf("This header is too big\n");
		printf("Can't processing\n");
		goto error;
	}

	int is_elf = cf_header.base.m_magic == 0x464C457F; // TODO:support raw elf

	pHeader = malloc(cf_header.header_v3.m_header_length);
	if(pHeader == NULL){
		printf("Cannot allocate memory for self header\n");
		goto error;
	}

	lseek(fd, 0, SEEK_SET);
	readbyte = read(fd, pHeader, cf_header.header_v3.m_header_length);

	if(readbyte != cf_header.header_v3.m_header_length){
		printf("self header : readbyte != cf_header.header_v3.m_header_length\n");
		goto error;
	}

	void *current_ptr = pHeader;

	cf_header_t *pCfHeader = (cf_header_t *)current_ptr;
	current_ptr = (void *)(((uintptr_t)current_ptr) + sizeof(cf_header_t));

	ext_header *pExtHeader = (ext_header *)current_ptr;
	current_ptr = (void *)(((uintptr_t)current_ptr) + sizeof(ext_header));

	pAppInfo = (SCE_appinfo *)malloc(sizeof(SCE_appinfo));
	if(pAppInfo == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(pAppInfo, (SCE_appinfo *)((uintptr_t)pHeader + pExtHeader->appinfo_offset), sizeof(SCE_appinfo));

	Elf32_Ehdr *pEhdr = (Elf32_Ehdr *)((uintptr_t)pHeader + pExtHeader->elf_offset);

	pPhdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * pEhdr->e_phnum);
	if(pPhdr == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(pPhdr, (Elf32_Phdr *)((uintptr_t)pHeader + pExtHeader->phdr_offset), sizeof(Elf32_Phdr) * pEhdr->e_phnum);

	pSegmentInfo = (segment_info *)malloc(sizeof(segment_info) * pEhdr->e_phnum);
	if(pSegmentInfo == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(pSegmentInfo, (segment_info *)((uintptr_t)pHeader + pExtHeader->section_info_offset), sizeof(Elf32_Phdr) * pEhdr->e_phnum);

	pVersion = (SCE_version *)malloc(sizeof(SCE_version));
	if(pVersion == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(pVersion, (SCE_version *)((uintptr_t)pHeader + pExtHeader->sceversion_offset), sizeof(SCE_version));

	pControlInfo = malloc(pExtHeader->controlinfo_size);
	if(pControlInfo == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(pControlInfo, (void *)((uintptr_t)pHeader + pExtHeader->controlinfo_offset), pExtHeader->controlinfo_size);


/*
	printf("elf segments info\n");
	printf("segment num : 0x%X\n", pEhdr->e_phnum);
*/
	for(int i=0;i<pEhdr->e_phnum;i++){
/*
		printf("[%d] type  :0x%08X offset:0x%08X vaddr:0x%08X paddr:0x%08X\n",
			i,
			pPhdr[i].p_type,
			pPhdr[i].p_offset,
			pPhdr[i].p_vaddr,
			pPhdr[i].p_paddr
		);
		printf("    filesz:0x%08X memsz :0x%08X flags:0x%08X align:0x%08X\n",
			pPhdr[i].p_filesz,
			pPhdr[i].p_memsz,
			pPhdr[i].p_flags,
			pPhdr[i].p_align
		);

		printf("offset=0x%08lX length=0x%08lX compression=%ld encryption=%ld\n", pSegmentInfo[i].offset, pSegmentInfo[i].length, pSegmentInfo[i].compression, pSegmentInfo[i].encryption);
*/
		context.segment[i].pData = malloc(pSegmentInfo[i].length);
		context.segment[i].memsz = pSegmentInfo[i].length;
		if(context.segment[i].pData == NULL){
			printf("Cannot allocate memory for segment\n");
			goto error;
		}

		lseek(fd, pSegmentInfo[i].offset, SEEK_SET);
		readbyte = read(fd, context.segment[i].pData, pSegmentInfo[i].length);
		if(readbyte != pSegmentInfo[i].length){
			printf("segment read error = 0x%X\n", readbyte);
			goto error;
		}
	}

	close(fd);
	fd = -1;

	context.is_elf       = is_elf;
	context.pHeader      = pHeader;
	context.pExtHeader   = pExtHeader;
	context.pEhdr        = pEhdr;
	context.pPhdr        = pPhdr;
	context.pSegmentInfo = pSegmentInfo;
	context.pAppInfo = pAppInfo;
	context.pVersion = pVersion;
	context.pControlInfo = pControlInfo;

	*ppResult = malloc(sizeof(context));
	if(*ppResult == NULL){
		printf("Cannot allocate memory\n");
		goto error;
	}

	memcpy(*ppResult, &context, sizeof(context));

	return 0;

error:
	if(fd >= 0){
		close(fd);
		fd = -1;
	}

	for(int i=0;i<5;i++){
		if(context.segment[i].pData != NULL)
			free(context.segment[i].pData);
	}

	if(pControlInfo != NULL){
		free(pControlInfo);
		pControlInfo = NULL;
	}

	if(pVersion != NULL){
		free(pVersion);
		pVersion = NULL;
	}

	if(pSegmentInfo != NULL){
		free(pSegmentInfo);
		pSegmentInfo = NULL;
	}

	if(pPhdr != NULL){
		free(pPhdr);
		pPhdr = NULL;
	}

	if(pAppInfo != NULL){
		free(pAppInfo);
		pAppInfo = NULL;
	}

	if(pHeader != NULL){
		free(pHeader);
		pHeader = NULL;
	}

	return -1;
}

int module_loader_save(ModuleLoaderContext *pContext, const char *path){

	if(pContext == NULL || path == NULL)
		return -1;

	int elf_header_size = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum;

	// printf("elf_header_size=0x%X\n", elf_header_size);

	int segment_offset0 = elf_header_size;
	int segment_offset1 = elf_header_size + 0x1000;

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){

		segment_offset0 = (segment_offset0 + (pContext->pPhdr[i].p_align - 1)) & ~(pContext->pPhdr[i].p_align - 1);
		segment_offset1 = (segment_offset1 + (pContext->pPhdr[i].p_align - 1)) & ~(pContext->pPhdr[i].p_align - 1);

		// printf("0x%08X/0x%08X\n", segment_offset0, segment_offset1);

		pContext->pPhdr[i].p_offset      = segment_offset0;
		pContext->pSegmentInfo[i].offset = segment_offset1;

		segment_offset0 += pContext->pPhdr[i].p_filesz;
		segment_offset1 += pContext->pSegmentInfo[i].length;
	}

	int fd;
	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, S_IRWXU);
	if(fd < 0){
		printf("cannot create output file\n");
		return fd;
	}

	uint64_t offset = sizeof(cf_header) + sizeof(ext_header);

	cf_header tmp_cf_header;
	memset(&tmp_cf_header, 0, sizeof(tmp_cf_header));
	memcpy(&tmp_cf_header, pContext->pHeader, sizeof(tmp_cf_header));

	tmp_cf_header.m_file_size           = segment_offset0;
	tmp_cf_header.m_certified_file_size = segment_offset1;

	write(fd, &tmp_cf_header, sizeof(tmp_cf_header));

	ext_header tmp_ext_header;
	memset(&tmp_ext_header, 0, sizeof(tmp_ext_header));

	tmp_ext_header.self_offset = 4;

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.appinfo_offset = offset;
	offset += sizeof(SCE_appinfo);

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.elf_offset = offset;
	offset += sizeof(Elf32_Ehdr);

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.phdr_offset = offset;
	offset += sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum;

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.shdr_offset = 0;
	offset += 0;

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.section_info_offset = offset;
	offset += sizeof(segment_info) * pContext->pEhdr->e_phnum;

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.sceversion_offset = offset;
	offset += sizeof(SCE_version);

	offset  = (offset + 0xF) & ~0xF;
	tmp_ext_header.controlinfo_offset = offset;
	tmp_ext_header.controlinfo_size = pContext->pExtHeader->controlinfo_size;
	offset += tmp_ext_header.controlinfo_size;

	write(fd, &tmp_ext_header, sizeof(tmp_ext_header));

	lseek(fd, tmp_ext_header.appinfo_offset, SEEK_SET);
	write(fd, pContext->pAppInfo, sizeof(SCE_appinfo));

	lseek(fd, tmp_ext_header.elf_offset, SEEK_SET);
	write(fd, pContext->pEhdr, sizeof(Elf32_Ehdr));

	lseek(fd, tmp_ext_header.phdr_offset, SEEK_SET);
	write(fd, pContext->pPhdr, ((sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum) + 0xF) & ~0xF);

	lseek(fd, tmp_ext_header.section_info_offset, SEEK_SET);
	write(fd, pContext->pSegmentInfo, sizeof(segment_info) * pContext->pEhdr->e_phnum);

	lseek(fd, tmp_ext_header.sceversion_offset, SEEK_SET);
	write(fd, pContext->pVersion, sizeof(SCE_version));

	lseek(fd, tmp_ext_header.controlinfo_offset, SEEK_SET);
	write(fd, pContext->pControlInfo, tmp_ext_header.controlinfo_size);

	lseek(fd, 0x1000, SEEK_SET);
	write(fd, pContext->pEhdr, sizeof(Elf32_Ehdr));
	write(fd, pContext->pPhdr, sizeof(Elf32_Phdr) * pContext->pEhdr->e_phnum);

	for(int i=0;i<pContext->pEhdr->e_phnum;i++){
		lseek(fd, pContext->pSegmentInfo[i].offset, SEEK_SET);
		write(fd, pContext->segment[i].pData, pContext->pSegmentInfo[i].length);
	}

	close(fd);

	return 0;
}
