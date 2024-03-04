/*
 * IPK - Project 2 (IOTA)
 * File: parser.h
 * Desc: Parser's header
 * Author: Roman Janota
 * Login: xjanot04
*/

#ifndef _PARSER_H_
#define _PARSER_H_

/* number of characters in maximum integer */
#define MAX_INT_LENGTH 10

/* number of maximum groupings into parentheses */
#define MAX_STACK_SIZE 100

/* binary tree */
struct node {
	char *value;
	struct node *left;
	struct node *right;
};

int tcp_parse_query(const char *query);

int udp_parse_request(const char *request, int bytes);

void del_tree(struct node *tree);

int new_tree(const char *expression, struct node **tree);

int calculate_answer(struct node *tree);

#endif
