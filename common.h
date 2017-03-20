#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define  log_inf(fmt, ...) printf("\033[1;37m%s %04u "fmt"\033[m", __FILE__, __LINE__, ##__VA_ARGS__)
#define  log_imp(fmt, ...) printf("\033[0;32;32m%s %04u "fmt"\033[m", __FILE__, __LINE__, ##__VA_ARGS__)
#define  log_wrn(fmt, ...) printf("\033[0;35m%s %04u "fmt"\033[m", __FILE__, __LINE__, ##__VA_ARGS__)
#define  log_err(fmt, ...) printf("\033[0;32;31m%s %04u "fmt"\033[m", __FILE__, __LINE__, ##__VA_ARGS__)




#define check_fd(x)		if (x <= 0){\
							log_err("%s failed", #x);\
							goto end;\
						}\


#define check_rst(x)	if (x != 0){\
							char msg[512] = {0};\
							strerror_r(errno, msg, sizeof(msg));\
							log_err("%s failed, error msg=[%s]", #x, msg);\
							goto end;\
						}\



#endif
