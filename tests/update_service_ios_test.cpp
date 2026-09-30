#include <TargetConditionals.h>
#undef TARGET_OS_IPHONE
#undef TARGET_OS_TV
#define TARGET_OS_IPHONE 1
#define TARGET_OS_TV 0
#include "update_service_test.cpp"
