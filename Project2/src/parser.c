/*
 * IPK - Project 2 (IOTA)
 * File: parser.c
 * Desc: Parsing client's messages and creating responses to them
 * Author: Roman Janota
 * Login: xjanot04
*/

#define _GNU_SOURCE

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "server.h"

static int
parse_operator(const char *operator, int *pos)
{
	if (operator[*pos] == '+' || operator[*pos] == '-' || operator[*pos] == '*' || operator[*pos] == '/') {
		/* found correct symbol */
		(*pos)++;
		return 0;
	}

	return -1;
}

static int
parse_sp(const char *sp, int *pos)
{
	if (sp[*pos] == ' ') {
		/* found a single space */
		(*pos)++;
		return 0;
	}

	return -1;
}

/* parses expr = "(" operator 2*(SP expr) ")" / 1*DIGIT */
static int
parse_expr(const char *expr, int *pos)
{
	if (isdigit(expr[*pos])) {
		/* digit */
		(*pos)++;
		while (isdigit(expr[*pos])) {
			(*pos)++;
		}

		return 0;
	} else if (expr[*pos] == '(') {
		/* inside bracket, parse in order */
		(*pos)++;

		if (parse_operator(expr, pos)) {
			return -1;
		}

		if (parse_sp(expr, pos)) {
			return -1;
		}

		if (parse_expr(expr, pos)) {
			return -1;
		}

		if (parse_sp(expr, pos)) {
			return -1;
		}

		if (parse_expr(expr, pos)) {
			return -1;
		}

		if (expr[*pos] != ')') {
			return -1;
		}

		(*pos)++;
		return 0;
	} else {
		/* unexpected token */
		return -1;
	}
}

/* parses solve = "SOLVE" SP query LF */
int
tcp_parse_query(const char *query)
{
	int pos = 0;

	/* check SOLVE */
	if (strncmp(query, "SOLVE ", 6)) {
		return 1;
	}

	/* check expression */
	pos += strlen("SOLVE ");
	if (parse_expr(query, &pos)) {
		return 1;
	}

	/* check LF */
	if (query[pos] != '\n') {
		return 1;
	}

	return 0;
}

/* UDP parser */
int
udp_parse_request(const char *request, int bytes)
{
	int pos = 0;

	if (bytes < 2) {
		/* check shortest length possible for a valid message */
		ERR("Message too short.");
		return -1;
	}

	if (request[0]) {
		/* check if it's request */
		ERR("Expected opcode to be request.");
		return -1;
	}

	if (parse_expr(request + 2, &pos)) {
		return -1;
	}

	return request[1];
}

/* creates a new tree node */
struct node *
new_node(char *value) {
  struct node *node = NULL;

  node = malloc(sizeof *node);
  if (!node) {
  	ERR("Memory allocation error.");
  	return NULL;
  }

  node->value = strdup(value);
  if (!node->value) {
  	ERR("Memory allocation error.");
  	return NULL;
  }
  node->left = NULL;
  node->right = NULL;

  return node;
}

/* frees the tree */
void
del_tree(struct node *tree)
{
	if (!tree) {
		return;
	}

	del_tree(tree->left);
	del_tree(tree->right);
	free(tree->value);
	free(tree);
}

static char *
build_tree(struct node **tree, char *expr)
{
  char buffer[MAX_INT_LENGTH + 1] = {0};
  struct node *node;
  int digit_count = 0;
  int is_digit = 0;

  if (*expr == '\0') {
    return '\0';
  }

  if (!*tree) {
    if (*expr == ' ') {
      expr++;
    }

    buffer[0] = *expr;

    if (isdigit(*expr)) {
      digit_count = 0;
      is_digit = 1;
      while (isdigit(*expr)) {
        buffer[digit_count] = *expr;
        digit_count++;
        expr++;
      }

      buffer[digit_count] = '\0';
    }

    node = new_node(buffer);
    if (!node) {
      return NULL;
    }

    *tree = node;

    if (is_digit) {
      return expr;
    }
  }

  if (*expr == '\0') {
    return '\0';
  }

  expr = build_tree(&(*tree)->left, expr + 1);
  expr = build_tree(&(*tree)->right, expr + 1);

  return expr;
}


/* tree constructor */
int
new_tree(const char *expression, struct node **tree)
{
	int i, j;
  int len = strlen(expression);
  char stripped[len];

  /* strip ws and parentheses */
  for (i = 0, j = 0; i < len; i++) {
    if (expression[i] != '(' && expression[i] != ')' && expression[i] != '\n') {
      stripped[j] = expression[i];
      j++;
    }
  }

  stripped[j] = '\0';

  build_tree(tree, stripped);

	return 0;
}

/* calculates the answer from the tree
 * returns -1 on error
 */
int
calculate_answer(struct node *tree)
{
	int left_val, right_val;

	if (!tree->left && !tree->right) {
		/* it's a leaf */
		return atoi(tree->value);
	} else {
		left_val = calculate_answer(tree->left);
		right_val = calculate_answer(tree->right);
		if (left_val == -1 || right_val == -1) {
			ERR("Calculation failed.");
			return -1;
		}

		if (!strcmp(tree->value, "+")) {
			return left_val + right_val;
		} else if (!strcmp(tree->value, "-")) {
			return left_val - right_val;
		} else if (!strcmp(tree->value, "*")) {
			return left_val * right_val;
		} else {
			if (right_val == 0) {
				ERR("Division by zero attempted.");
				return INT_MIN;
			}
			return left_val / right_val;
		}
	}
}
