#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "linked_list.h"

int main()
{
	struct linked_list * list = linked_list_init(free);

	int a = 9;
	int b = 10;
	int c = 11;

	linked_list_append(list, &a);
	linked_list_append(list, &b);
	linked_list_append(list, &c);

	printf("BEFORE\n");
	int i;
	for (i = 0; i < list->len; i++) {
		printf("list[%d] = %d\n",i, *((int*)linked_list_get(list, i)));
	}

	for (i = 0; i < list->len; i++) {
		if (*((int*)linked_list_get(list, i)) == 10) {
			linked_list_remove(list, i);
printf("removed %d\n", i);
			break;
		}
	}

	printf("AFTER\n");
	for (i = 0; i < list->len; i++) {
		printf("list[%d] = %d\n", i, *((int*)linked_list_get(list,i)));
	}

	return 0;
}
