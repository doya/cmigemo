/* vi:set ts=8 sts=4 sw=4 tw=0: */
/*
 * romaji.h - ���[�}���ϊ�
 *
 * Written By:  MURAOKA Taro <koron@tka.att.ne.jp>
 * Last Change: 04-May-2004.
 */

#ifndef ROMAJI_H
#define ROMAJI_H

typedef struct _romaji romaji;

#ifdef __cplusplus
extern "C" {
#endif

romaji* romaji_open();
void romaji_close(romaji* object);
int romaji_add_table(romaji* object, const unsigned char* key,
	const unsigned char* value);
int romaji_load(romaji* object, const unsigned char* filename);
unsigned char* romaji_convert(romaji* object, const unsigned char* string,
	unsigned char** ppstop);
unsigned char* romaji_convert2(romaji* object, const unsigned char* string,
	unsigned char** ppstop, int ignorecase);
void romaji_release(romaji* object, unsigned char* string);

#ifdef __cplusplus
}
#endif

#endif /* ROMAJI_H */
