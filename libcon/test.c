#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/content_name.h"

int main()
{
	struct content_name * base = content_name_create("/music/live_like");
	printf("full_name = (%s), len = %d, strlen = %lu\n", base->full_name, base->len, strlen(base->full_name));
	content_name_printComponents(base);
	char comp[MAX_NAME_LENGTH];
	snprintf(comp, MAX_NAME_LENGTH, "%d", 5831);
	content_name_appendComponent(base, comp);
	printf("full_name = (%s), len = %d, strlen = %lu\n", base->full_name, base->len, strlen(base->full_name));
	content_name_printComponents(base);
	content_name_removeComponent(base, base->num_components - 1);
	snprintf(comp, MAX_NAME_LENGTH, "%d", 0);
	content_name_appendComponent(base, comp);
	printf("full_name = (%s), len = %d, strlen = %lu\n", base->full_name, base->len, strlen(base->full_name));
	content_name_printComponents(base);

	struct content_name * name = content_name_create("/test/tom/foo");
	printf("len = %d\n", name->len);

	content_name_appendComponent(name, "bar");
	printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
	content_name_printComponents(name);
	content_name_removeComponent(name, 2);
	printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
	content_name_printComponents(name);
	content_name_removeComponent(name, 2);
	printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
	content_name_printComponents(name);
	content_name_removeComponent(name, 0);
	printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
	content_name_printComponents(name);
	content_name_appendComponent(name, "0");
	printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
	content_name_printComponents(name);
	
	int i;
	char str[MAX_NAME_LENGTH];
	for (i = 0; i < 20; i++) {
		sprintf(str, "%d", i);
		content_name_removeComponent(name, name->num_components-1);
		content_name_appendComponent(name, str);
		printf("full_name = (%s), len = %d, strlen = %lu\n", name->full_name, name->len, strlen(name->full_name));
		content_name_printComponents(name);
	}
}
