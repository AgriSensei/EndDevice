#ifndef _LOGGING_HPP
#define _LOGGING_HPP

//#define DO_LOGGING

#ifdef DO_LOGGING
#define LOG(x)           \
    if (Serial) {        \
        Serial.print(x); \
    }
#define LOGLN(x)           \
    if (Serial) {          \
        Serial.println(x); \
    }
#else
#define LOG(x)
#define LOGLN(x)
#endif

#endif
