/*
 * psp2rela
 * Copyright (C) 2021, Princess of Sleeping
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sysio.h"
#include "debug.h"

int rela_debug_level = RELA_DBG_LOG_LEVEL_NONE;

#define RELA_LOG_BUFFER_SIZE (0x1000)

int log_fd = -1;
char log_buffer[RELA_LOG_BUFFER_SIZE];
unsigned int log_write_size;
int is_rela_show_mode;

int log_open(const char *path){

	if(log_fd >= 0)
		return -1;

	log_fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, S_IRWXU);
	if(log_fd < 0){
		return log_fd;
	}

	log_write_size = 0;
	memset(log_buffer, 0, RELA_LOG_BUFFER_SIZE);

	return 0;
}

int log_close(void){

	if(log_fd < 0)
		return -1;

	if(log_write_size > 0){
		write(log_fd, log_buffer, log_write_size);
		memset(log_buffer, 0, RELA_LOG_BUFFER_SIZE);
		log_write_size = 0;
	}

	close(log_fd);
	log_fd = -1;

	return 0;
}

int log_write(const char *fmt, ...){

	int res;
	unsigned int len, buffer_remain = 0;
	va_list args;
	char string[0x200];

	if(log_fd < 0)
		return -1;

	memset(string, 0, sizeof(string));
	va_start(args, fmt);
	len = vsnprintf(string, sizeof(string) - 1, fmt, args);
	va_end(args);

	if((log_write_size + len) > RELA_LOG_BUFFER_SIZE){

		buffer_remain = RELA_LOG_BUFFER_SIZE - log_write_size;

		memcpy(&log_buffer[log_write_size], string, buffer_remain);
		write(log_fd, log_buffer, RELA_LOG_BUFFER_SIZE);
		memset(log_buffer, 0, RELA_LOG_BUFFER_SIZE);
		log_write_size = 0;
		len -= buffer_remain;
	}

	memcpy(&log_buffer[log_write_size], &string[buffer_remain], len);
	log_write_size += len;

	res = 0;

	return res;
}

int rela_debug_init(const char *args, const char *path){

	rela_debug_level = RELA_DBG_LOG_LEVEL_NONE;
	is_rela_show_mode = 0;

	if(args == NULL)
		return 0;

	int log2file = 0;

	while(*args != 0){

		switch(*args){
		case 'v':
			if(rela_debug_level != RELA_DBG_LOG_LEVEL_TRACE)
				rela_debug_level--;
			break;
		case 'f':
			log2file = (path != NULL);
			break;
		case 's':
			is_rela_show_mode = 1;
			break;
		default:
			printf("%s:Unknown args char = %c\n", __FUNCTION__, *args);
			break;
		}

		args++;
	}

	if(log2file != 0){
		log_open(path);
	}

	return 0;
}

int rela_debug_fini(void){
	log_close();
	return 0;
}

int rela_is_show_mode(void){
	return is_rela_show_mode != 0;
}

int printf_internal(int level, const char *fmt, va_list args){

	char string[0x200];

	if(level < rela_debug_level)
		return 0;

	memset(string, 0, sizeof(string));
	vsnprintf(string, sizeof(string) - 1, fmt, args);

	if(log_fd < 0){
		printf("%s", string);
	}else{
		log_write("%s", string);
	}

	return 0;
}

int printf_t(const char *fmt, ...){

	int res;
	va_list args;

	va_start(args, fmt);
	res = printf_internal(RELA_DBG_LOG_LEVEL_TRACE, fmt, args);
	va_end(args);

	return res;
}

int printf_d(const char *fmt, ...){

	int res;
	va_list args;

	va_start(args, fmt);
	res = printf_internal(RELA_DBG_LOG_LEVEL_DEBUG, fmt, args);
	va_end(args);

	return res;
}

int printf_i(const char *fmt, ...){

	int res;
	va_list args;

	va_start(args, fmt);
	res = printf_internal(RELA_DBG_LOG_LEVEL_INFO, fmt, args);
	va_end(args);

	return res;
}

int printf_w(const char *fmt, ...){

	int res;
	va_list args;

	va_start(args, fmt);
	res = printf_internal(RELA_DBG_LOG_LEVEL_WARNING, fmt, args);
	va_end(args);

	return res;
}

int printf_e(const char *fmt, ...){

	int res;
	va_list args;

	va_start(args, fmt);
	res = printf_internal(RELA_DBG_LOG_LEVEL_ERROR, fmt, args);
	va_end(args);

	return res;
}

