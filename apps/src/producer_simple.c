#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccnu.h"
#include "ts.h"

int main()
{
	struct timespec now;
	ts_fromnow(&now);	
	
	char * str = "Hello World!! ~test content";
	struct content_obj con;
	con.name = content_name_create("/test/hello");
	con.timestamp = now.tv_sec;	
	con.size = 	strlen(str);
	con.data = (uint8_t * ) malloc(strlen(str));
	memcpy(con.data, str, con.size);
	
	exit(ccnu_publish(&con));
}