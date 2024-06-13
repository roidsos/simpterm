#ifndef __STD_H__
#define __STD_H__

//unsigned
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
//signed
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef unsigned long uptr;

typedef unsigned char _bool; // so limine.h wont fuck with my kernel

#define true 1
#define false 0


#define NULL ((void *)0)

#endif // __STD_H__