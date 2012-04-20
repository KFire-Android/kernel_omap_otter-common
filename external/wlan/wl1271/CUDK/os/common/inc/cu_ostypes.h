#ifndef _CUOSTYPES_H_
#define _CUOSTYPES_H_

/* types */
/*********/
#if !defined(VOID)
typedef void                    VOID,*PVOID;
#endif
typedef unsigned char           U8,*PU8;
typedef /*signed*/ char         S8,*PS8,**PPS8;
typedef unsigned short          U16,*PU16;
typedef signed short            S16,*PS16;
typedef unsigned long           U32,*PU32;
typedef signed long             S32,*PS32;
typedef float                   F32,*PF32;
typedef PVOID                   THandle;
typedef int                     TI_SIZE_T;

#endif
