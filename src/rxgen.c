/* vim:set ts=8 sts=4 sw=4 tw=0 */
/*
 * rxgen.c - regular expression generator
 *
 * Written By:  Muraoka Taro <koron@tka.att.ne.jp>
 * Last Change: 08-Aug-2001.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wordbuf.h"
#include "rxgen.h"

#define RXGEN_ENC_SJISTINY
//#define RXGEN_OP_VIM

#define RXGEN_OP_MAXLEN 8
#define RXGEN_OP_OR "|"
#define RXGEN_OP_NEST_IN "("
#define RXGEN_OP_NEST_OUT ")"
#define RXGEN_OP_SELECT_IN "["
#define RXGEN_OP_SELECT_OUT "]"
#define RXGEN_OP_NEWLINE ""

int n_rnode_new = 0;
int n_rnode_delete = 0;

typedef struct _rnode rnode;

struct _rxgen
{
    rnode *node;
    RXGEN_PROC_CHAR2INT char2int;
    RXGEN_PROC_INT2CHAR int2char;
    unsigned char op_or[RXGEN_OP_MAXLEN];
    unsigned char op_nest_in[RXGEN_OP_MAXLEN];
    unsigned char op_nest_out[RXGEN_OP_MAXLEN];
    unsigned char op_select_in[RXGEN_OP_MAXLEN];
    unsigned char op_select_out[RXGEN_OP_MAXLEN];
    unsigned char op_newline[RXGEN_OP_MAXLEN];
};

/*
 * rnode interfaces
 */

struct _rnode
{
    unsigned int code;
    rnode* child;
    rnode* next;
};

    static rnode*
rnode_new()
{
    ++n_rnode_new;
    return (rnode*)calloc(1, sizeof(rnode));
}

    static void
rnode_delete(rnode* node)
{
    while (node)
    {
	rnode* child = node->child;
	if (node->next)
	    rnode_delete(node->next);
	free(node);
	node = child;
	++n_rnode_delete;
    }
}

/*
 * rxgen interfaces
 */

    static int
default_char2int(unsigned char* in, unsigned int* out)
{
#if defined(RXGEN_ENC_SJISTINY)
    if (*in >= 0x80)
    {
	*out = (unsigned int)in[0] << 8 | (unsigned int)in[1];
	return 2;
    }
    else
    {
	*out = *in;
	return 1;
    }
#else
    *out = *in;
    return 1;
#endif
}

    static int
default_int2char(unsigned int in, unsigned char* out)
{
    /* outは最低でも16バイトある、という仮定を置く */
#if defined(RXGEN_ENC_SJISTINY)
    if (in >= 0x100)
    {
	if (out)
	{
	    out[0] = (unsigned char)((in >> 8) & 0xFF);
	    out[1] = (unsigned char)(in & 0xFF);
	}
	return 2;
    }
    else
    {
	int len = 0;
	switch (in)
	{
	    case '\\':
	    case '.': case '*': case '^': case '$': case '/':
#ifdef RXGEN_OP_VIM
	    case '[': case ']': case '~':
#endif
		if (out)
		    out[len++] = '\\';
	    default:
		if (out)
		    out[len++] = (unsigned char)(in & 0xFF);
		break;
	}
	return len;
    }
#else
    if (out)
	out[0] = (unsigned char)(in & 0xFF);
    return 1;
#endif
}

    void
rxgen_setproc_char2int(rxgen* object, RXGEN_PROC_CHAR2INT proc)
{
    if (object)
	object->char2int = proc ? proc : default_char2int;
}

    void
rxgen_setproc_int2char(rxgen* object, RXGEN_PROC_INT2CHAR proc)
{
    if (object)
	object->int2char = proc ? proc : default_int2char;
}

    rxgen*
rxgen_open()
{
    rxgen* object = (rxgen*)calloc(1, sizeof(rxgen));
    if (object)
    {
	rxgen_setproc_char2int(object, NULL);
	rxgen_setproc_int2char(object, NULL);
	strcpy(object->op_or,		RXGEN_OP_OR);
	strcpy(object->op_nest_in,	RXGEN_OP_NEST_IN);
	strcpy(object->op_nest_out,	RXGEN_OP_NEST_OUT);
	strcpy(object->op_select_in,	RXGEN_OP_SELECT_IN);
	strcpy(object->op_select_out,	RXGEN_OP_SELECT_OUT);
	strcpy(object->op_newline,	RXGEN_OP_NEWLINE);
    }
    return object;
}

    void
rxgen_close(rxgen* object)
{
    if (object)
    {
	rnode_delete(object->node);
	free(object);
    }
}

    int
rxgen_add(rxgen* object, unsigned char* word)
{
    rnode **ppnode;

    if (!object || !word)
	return 0;

    ppnode = &object->node;
    while (1)
    {
	unsigned int code;
	int len = object->char2int(word, &code);

	/* 終了条件 */
	if (!code || !len)
	{
	    /* 入力パターンよりも長い既存パターンは破棄する */
	    if (*ppnode)
	    {
		rnode_delete(*ppnode);
		*ppnode = NULL;
	    }
	    break;
	}

	while (*ppnode && (*ppnode)->code != code)
	    ppnode = &(*ppnode)->next;
	if (*ppnode)
	{
	    /* 既存パターンより長い入力パターンは破棄する */
	    if (!(*ppnode)->child)
		break;
	}
	else
	{
	    /* 未知の長いパターンを辿って記憶する。 */
	    *ppnode = rnode_new();
	    (*ppnode)->code = code;
	}
	ppnode = &(*ppnode)->child;
	word += len;
    }
    return 1;
}

    static void
rxgen_generate_stub(rxgen* object, wordbuf* buf, rnode* node)
{
    unsigned char ch[16];
    int chlen, nochild, haschild = 0, brother = 1;
    rnode *tmp;

    /* 現在の階層の特性(兄弟の数、子供の数)をチェックする */
    for (tmp = node; tmp; tmp = tmp->next)
    {
	if (tmp->next)
	    ++brother;
	if (tmp->child)
	    ++haschild;
    }
    nochild = brother - haschild;

    /* 必要ならば()によるグルーピング */
    if (brother > 1 && haschild > 0)
	wordbuf_cat(buf, object->op_nest_in);
#if 1
    /* 子の無いノードを先に[]によりグルーピング */
    if (nochild > 0)
    {
	if (nochild > 1)
	    wordbuf_cat(buf, object->op_select_in);
	for (tmp = node; tmp; tmp = tmp->next)
	{
	    if (tmp->child)
		continue;
	    chlen = object->int2char(tmp->code, ch);
	    ch[chlen] = '\0';
	    /*printf("nochild: %s\n", ch);*/
	    wordbuf_cat(buf, ch);
	}
	if (nochild > 1)
	    wordbuf_cat(buf, object->op_select_out);
    }
#endif
#if 1
    /* 子のあるノードを出力 */
    if (haschild > 0)
    {
	/* グループを出力済みならORで繋ぐ */
	if (nochild > 0)
	    wordbuf_cat(buf, object->op_or);
	for (tmp = node; !tmp->child; tmp = tmp->next)
	    ;
	while (1)
	{
	    chlen = object->int2char(tmp->code, ch);
	    ch[chlen] = '\0';
	    wordbuf_cat(buf, ch);
	    /*printf("haschild: %s(brother=%d, haschild=%d)\n", ch, brother, haschild);*/
	    /* 空白・改行飛ばしのパターンを挿入 */
	    if (object->op_newline[0])
		wordbuf_cat(buf, object->op_newline);
	    rxgen_generate_stub(object, buf, tmp->child);
	    for (tmp = tmp->next; tmp && !tmp->child; tmp = tmp->next)
		;
	    if (!tmp)
		break;
	    if (haschild > 1)
		wordbuf_cat(buf, object->op_or);
	}
    }
#endif
    /* 必要ならば()によるグルーピング */
    if (brother > 1 && haschild > 0)
	wordbuf_cat(buf, object->op_nest_out);
}

    unsigned char*
rxgen_generate(rxgen* object)
{
    unsigned char* answer = NULL;
    wordbuf *buf;

    if (object && (buf = wordbuf_open()))
    {
	if (object->node)
	    rxgen_generate_stub(object, buf, object->node);
	answer = strdup(wordbuf_get(buf));
	wordbuf_close(buf);
    }
    return answer;
}

    void
rxgen_release(rxgen* object, unsigned char* string)
{
    free(string);
}

/*
 * rxgen_add()してきたパターンを全てリセット。
 */
    void
rxgen_reset(rxgen* object)
{
    if (object)
    {
	rnode_delete(object->node);
	object->node = NULL;
    }
}

    static unsigned char*
rxgen_get_operator_stub(rxgen* object, int index)
{
    switch (index)
    {
	case RXGEN_OPINDEX_OR:
	    return object->op_or;
	case RXGEN_OPINDEX_NEST_IN:
	    return object->op_nest_in;
	case RXGEN_OPINDEX_NEST_OUT:
	    return object->op_nest_out;
	case RXGEN_OPINDEX_SELECT_IN:
	    return object->op_select_in;
	case RXGEN_OPINDEX_SELECT_OUT:
	    return object->op_select_out;
	case RXGEN_OPINDEX_NEWLINE:
	    return object->op_newline;
	default:
	    return NULL;
    }
}

    const unsigned char*
rxgen_get_operator(rxgen* object, int index)
{
    return (const unsigned char*)
	(object ? rxgen_get_operator_stub(object, index) : NULL);
}

    int
rxgen_set_operator(rxgen* object, int index, unsigned char* op)
{
    unsigned char* dest;

    if (!object)
	return 1; /* Invalid object */
    if (strlen(op) >= RXGEN_OP_MAXLEN)
	return 2; /* Too long operator */
    if (!(dest = rxgen_get_operator_stub(object, index)))
	return 3; /* No such an operator */
    strcpy(dest, op);

    return 0;
}

#if 0
/*
 * main
 */
    int
main(int argc, char** argv)
{
    rxgen *prx;

    if (prx = rxgen_open())
    {
	char buf[256], *ans;

	while (gets(buf) && !feof(stdin))
	    rxgen_add(prx, buf);
	ans = rxgen_generate(prx);
	printf("rxgen=%s\n", ans);
	rxgen_release(prx, ans);
	rxgen_close(prx);
    }
    fprintf(stderr, "n_rnode_new=%d\n", n_rnode_new);
    fprintf(stderr, "n_rnode_delete=%d\n", n_rnode_delete);
}
#endif
