#include <stdio.h>
#include "stb_ds.h"
#include "iced.h"

static void trace(struct node* node)
{
	const int n = arrlen(node->child_arr);
	for (int i = 0; i < n; i++) {
		trace(&node->child_arr[i]);
	}
}

void iced_codegen(struct node* root)
{
	//printf("CODEGEN\n");
	trace(root);
}
