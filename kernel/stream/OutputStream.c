#include <stream/OutputStream.h>

extern OutputStream uartOutputStream;

OutputStream* currentOutputStream = &uartOutputStream;
