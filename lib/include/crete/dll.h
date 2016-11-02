#ifndef CRETE_DLL_H
#define CRETE_DLL_H

#define CRETE_DLL_IMPORT __attribute__ ((visibility ("default")))
#define CRETE_DLL_EXPORT __attribute__ ((visibility ("default")))
#define CRETE_DLL_LOCAL  __attribute__ ((visibility ("hidden")))

#endif // CRETE_DLL_H
